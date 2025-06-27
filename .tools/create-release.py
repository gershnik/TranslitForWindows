# Copyright (c) 2023, Eugene Gershnik
# SPDX-License-Identifier: GPL-3.0-or-later


import sys
import re
import subprocess

from pathlib import Path
from datetime import date

MYPATH = Path(__file__).parent
ROOT = MYPATH.parent

NEW_VER = sys.argv[1]

VER_COMPONENTS = [int(i) for i in NEW_VER.split('.')]

if len(VER_COMPONENTS) > 3:
    print('Invalid number of version components', file=sys.stderr)
    sys.exit(1)

VER_MAJOR = (VER_COMPONENTS[0:] + [0])[0]
VER_MINOR = (VER_COMPONENTS[1:] + [0])[0]
VER_PATCH = (VER_COMPONENTS[2:] + [0])[0]

unreleased_link_pattern = re.compile(r"^\[Unreleased\]: (.*)$", re.DOTALL)
lines = []
with open(ROOT / "CHANGELOG.md", "rt", encoding='utf-8') as change_log:
    for line in change_log.readlines():
        # Move Unreleased section to new version
        if re.fullmatch(r"^## Unreleased.*$", line, re.DOTALL):
            lines.append(line)
            lines.append("\n")
            lines.append(
                f"## [{NEW_VER}] - {date.today().isoformat()}\n"
            )
        else:
            lines.append(line)
    lines.append(f'[{NEW_VER}]: https://github.com/gershnik/TranslitForWindows/releases/v{NEW_VER}\n')

with open(ROOT / "CHANGELOG.md", "wt", encoding='utf-8', newline='\n') as change_log:
    change_log.writelines(lines)


lines = []
with open(ROOT / 'Version.props', 'r', encoding='utf-8') as ver_file:
    for line in ver_file:
        line = re.sub(r'^(\s*<BuildMajorVersion>\s*)\d+(\s*</BuildMajorVersion>\s*)$', fr'\g<1>{VER_MAJOR}\2', line)
        line = re.sub(r'^(\s*<BuildMinorVersion>\s*)\d+(\s*</BuildMinorVersion>\s*)$', fr'\g<1>{VER_MINOR}\2', line)
        line = re.sub(r'^(\s*<BuildPatchVersion>\s*)\d+(\s*</BuildPatchVersion>\s*)$', fr'\g<1>{VER_PATCH}\2', line)
        lines.append(line)

with open(ROOT / 'Version.props', "wt", encoding='utf-8', newline='\n') as ver_file:
    ver_file.writelines(lines)


subprocess.run(['git', 'add',
                ROOT / "CHANGELOG.md",
                ROOT / "Version.props"], check=True)
subprocess.run(['git', 'commit', '-m', f'chore: creating version {NEW_VER}'], check=True)
subprocess.run(['git', 'tag', f'v{NEW_VER}'], check=True)
