# Verification Matrix MCSOS 260502 — M0

| Requirement | Verification command | Expected evidence | Pass/Fail |
|---|---|---|---|
| REQ-M0-001 | `pwd` | Path berada di `/home/agung/src/mcsos` | Pass |
| REQ-M0-002 | `bash tools/check_env.sh` | Semua tool wajib `[OK]` | Pass |
| REQ-M0-003 | `cat build/meta/toolchain-versions.txt` | Versi tool tercatat | Pass |
| REQ-M0-004 | `tree -a -L 3` | Struktur docs/tools/smoke/build tersedia | Pass |
| REQ-M0-005 | `make smoke` | Object ELF64 x86-64 relocatable | Pass |
| REQ-M0-006 | `test -s docs/requirements/assumptions_and_nongoals.md` | File ada dan tidak kosong | Pass |
| REQ-M0-007 | `test -s docs/adr/ADR-0001-toolchain-and-boot-baseline.md` | File ada dan tidak kosong | Pass |
| REQ-M0-008 | `test -s docs/security/threat_model.md` | File ada dan tidak kosong | Pass |
| REQ-M0-009 | `test -s docs/governance/risk_register.md` | File ada dan tidak kosong | Pass |
| REQ-M0-010 | `test -s docs/testing/verification_matrix.md` | File ada dan tidak kosong | Pass |
| REQ-M0-011 | `git log --oneline -n 3` | Minimal satu commit M0 | Pass |
| REQ-M0-012 | `test -s docs/reports/M0-laporan.md` | Laporan tersedia | Pass |
