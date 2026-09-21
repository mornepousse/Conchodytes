#!/usr/bin/env bash
# Host test of the board's pinout contract (boards/conchodytes/test_pins.c),
# compiled against the firmware's test framework — no toolchain needed.
set -euo pipefail
cd "$(dirname "$0")/.."
F=firmware
[ -f "$F/test/test_framework.h" ] || { echo "firmware submodule missing: git submodule update --init" >&2; exit 1; }
tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT
cat > "$tmp/main.c" <<'EOC'
#include <stdio.h>
int _test_pass_count = 0, _test_fail_count = 0;
void test_conchodytes_pins(void);
int main(void) { test_conchodytes_pins(); printf("Results: %d passed, %d failed\n", _test_pass_count, _test_fail_count); return _test_fail_count ? 1 : 0; }
EOC
cc -std=c11 -Wall -Wno-unused-function -DTEST_HOST -I "$F/test" -I "$F/main/comm/rf" -I boards/conchodytes \
   boards/conchodytes/test_pins.c "$tmp/main.c" -o "$tmp/test_pins"
"$tmp/test_pins"
