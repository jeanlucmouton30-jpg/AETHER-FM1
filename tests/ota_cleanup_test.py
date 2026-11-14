#!/usr/bin/env python3
"""Regression guard: OTA cleanup must never scan user-save sectors."""

from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


class OtaCleanupTest(unittest.TestCase):
    def test_cleanup_is_limited_to_the_reserved_ota_marker(self):
        ota = (ROOT / "firmware/src/ota.c").read_text()
        loader = (ROOT / "firmware/loader/ldr_core.c").read_text()

        area = int(re.search(r"#define OTA_AREA\s+(0x[0-9A-Fa-f]+)u", ota).group(1), 16)
        length = int(re.search(r"#define OTA_AREA_LEN\s+(0x[0-9A-Fa-f]+)u", ota).group(1), 16)
        marker = area + length - 0x100
        sector = marker & ~0xFFF

        self.assertEqual(marker, 0xE4F00)
        self.assertEqual(sector, 0xE4000)
        self.assertGreaterEqual(marker, area)
        self.assertLess(marker + 80, area + length)

        match = re.search(
            r"static void ldr_records_drop\(void\)\s*\{(.*?)\n\}",
            loader,
            re.S,
        )
        self.assertIsNotNone(match, "ldr_records_drop introuvable")
        body = match.group(1)

        self.assertIn("ldr_fread(OTA_RES, r, sizeof r)", body)
        self.assertIn("ldr_erase(OTA_RES & ~0xFFFu)", body)
        self.assertNotRegex(body, r"\b(for|while)\s*\(")
        self.assertNotIn("LDR_REC_LO", loader)
        self.assertNotIn("LDR_REC_HI", loader)

        user_save_sectors = (0xDF000, 0xE5000, 0xE6000, 0xE7000)
        self.assertNotIn(sector, user_save_sectors)


if __name__ == "__main__":
    unittest.main()
