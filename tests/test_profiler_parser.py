"""Standard-library regression tests for offline profiler symbol resolution."""
import importlib.util
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.dont_write_bytecode = True
SPEC = importlib.util.spec_from_file_location(
    "profiler_parser", Path(__file__).resolve().parents[2] / "profiler" / "parse_prof_dump.py"
)
PARSER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PARSER)


class ProfilerParserTests(unittest.TestCase):
    def make_elf(self, folder, machine):
        path = Path(folder) / "firmware.elf"
        header = bytearray(20)
        header[:6] = b"\x7fELF\x01\x01"
        struct.pack_into("<H", header, 18, machine)
        path.write_bytes(header)
        return path

    def test_thumb_addresses_normalized_only_for_arm(self):
        for machine, expected in [(40, "0x08000100"), (62, "0x08000101")]:
            with self.subTest(machine=machine), tempfile.TemporaryDirectory() as folder:
                elf = self.make_elf(folder, machine)
                completed = subprocess.CompletedProcess([], 0, "workload\nsource.c:5\n", "")
                with patch.object(PARSER.subprocess, "run", return_value=completed) as run:
                    symbols = PARSER.resolve_symbols([0x08000101], elf, "addr2line")
                    self.assertEqual(expected, run.call_args.args[0][-1])
                    self.assertEqual("workload", symbols[0x08000101][0])

    def test_text_rows_resolved_without_replacing_diagnostics(self):
        text = "Function Calls\n0x08000101   5\nHealth 0x08000101\n"
        with patch.object(PARSER, "resolve_symbols", return_value={0x08000101: ("workload", "")}):
            result = PARSER.resolve_text_log(text, Path("firmware.elf"), "addr2line")
        self.assertEqual("Function Calls\nworkload   5\nHealth 0x08000101\n", result)

    def test_invalid_text_and_elf_are_errors(self):
        with self.assertRaises(ValueError):
            PARSER.resolve_text_log("no metrics", Path("unused"), "addr2line")
        with tempfile.TemporaryDirectory() as folder:
            elf = Path(folder) / "invalid.elf"
            elf.write_bytes(b"not an ELF")
            with self.assertRaises(ValueError):
                PARSER.resolve_symbols([1], elf, "addr2line")

    def test_v4_crc_and_truncation(self):
        header = struct.pack(PARSER.HEADER_V4_FORMAT, b"PROF", 4, 1, 1000000, 0, 0, 0, 0, 0, 1, 0, 1, 2)
        record = struct.pack(PARSER.RECORD_FORMAT, 0x08000101, 1, 100, 100, 100)
        payload = header + record
        crc = PARSER.calculate_crc32(0xFFFFFFFF, payload) ^ 0xFFFFFFFF
        with tempfile.TemporaryDirectory() as folder:
            capture = Path(folder) / "capture.bin"
            capture.write_bytes(payload + struct.pack("<I", crc))
            parsed, records, actual_crc = PARSER.read_binary_dump(capture)
            self.assertEqual(4, parsed.version)
            self.assertEqual(100, records[0].total_cycles)
            self.assertEqual(crc, actual_crc)
            capture.write_bytes(payload)
            with self.assertRaises(ValueError):
                PARSER.read_binary_dump(capture)


if __name__ == "__main__":
    unittest.main()
