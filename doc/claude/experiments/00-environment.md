# Stage 0: environment and bootstrap feasibility

Starting point: branch `master` at `b1043d68b`, `version.txt` = `8.0.2`.
All paths are relative to the repository root.

**Verdict:** the bootstrap works on Linux. The only failure seen is specific
to the sandbox these experiments ran in (it blocks GitHub source-archive
URLs); with a 40-line wrapper that fetches the same commits through `git`
the bootstrap completes. Total: about 4 minutes, 2.6 GB of disk.

## Machine

Commands: `uname -srm`, `. /etc/os-release; echo $PRETTY_NAME`, `nproc`,
`free -h`, `df -h .`, `lscpu`, `<tool> --version`.

| Item | Value |
|---|---|
| Kernel | Linux 6.18.44 x86_64 (virtual machine) |
| Distribution | Ubuntu 24.04.5 LTS |
| CPUs | 4 (Intel Xeon @ 2.10 GHz, 1 thread per core) |
| RAM | 15 GiB, no swap |
| Free disk | 60 GiB before, 57 GiB after the bootstrap |
| python3 | 3.13.18 (default `python3`); 3.12.3 and 3.11 also installed |
| gcc / g++ | 13.3.0 (Ubuntu 13.3.0-6ubuntu2~24.04.1) |
| clang | 18.1.3 |
| cmake | 3.28.3 |
| ninja | 1.11.1 |
| make | GNU Make 4.3 |
| git | 2.43.0 |
| ccache | not installed |
| pip | 24.0 |

OS packages named in the CI image (`utils/holly/Dockerfile`) and their state
here (`dpkg -l <name>`): `libusb-1.0-0` and `build-essential` installed;
`gettext`, `libjpeg-dev`, `python3-venv`, `python3-requests` **not**
installed. Nothing was installed for this stage.

OS packages installed later, all with `apt-get install -y` and each recorded
in the stage that needed it: `gettext` (stages 1 and 2), `ccache`
(stage 3, optional), `libglu1-mesa libglew2.2 libglut3.12` and
`gdb-multiarch` (stage 4).

Network: outbound HTTPS only, through a filtering proxy that re-terminates
TLS (tools must trust an extra CA bundle; `requests`, `pip`, `git` and `curl`
are pre-configured for it through the usual environment variables).

## Download hosts used by `utils/bootstrap.py`

Command: one `curl -sIL -o /dev/null -w '%{http_code}' <url>` per host, URLs
taken from the `dependencies` table in `utils/bootstrap.py`.

| Host / URL shape | Used for | HEAD result |
|---|---|---|
| `github.com/<owner>/<repo>/releases/download/...` | ninja, cmake, mini404, MMU firmware, ESP toolchains | 200 |
| `developer.arm.com` | `gcc-arm-none-eabi` 13.3.rel1 | 200 |
| `prusa-buddy-firmware-dependencies.s3.eu-central-1.amazonaws.com` | clang-format, bootloaders, AC controller firmware | 200 |
| `raw.githubusercontent.com` | CMSIS SVD files | 200 |
| `dl.espressif.com` | ESP SDK tool installers | 200 |
| `pypi.org` | pip packages | 200 |
| `github.com/<owner>/<repo>/archive/<ref>.zip` (redirects to `codeload.github.com`) | CrashDebug, ESP8266 RTOS SDK, ESP-IDF and 8 of their submodules | **403** |

The 403 is a policy of this sandbox, not of GitHub: source-archive downloads
are refused for repositories that are not attached to the session, while
release assets and `git` protocol reads of public repositories work. On an
ordinary Linux machine these URLs are public, so attempt 1 below is expected
to succeed there (not run on such a machine).

## Bootstrap, attempt 1: as documented

```bash
python3 utils/bootstrap.py
```

Result: **exit 1 after 197 s**.

- Creating `.venv` and installing `requirements.txt` into it: 146 s. Almost
  all of it is `numpy==1.26.4`, which has no wheel for Python 3.13 and is
  compiled from source (see the pip section).
- Downloading and unpacking ninja, cmake, the ARM toolchain, clang-format,
  10 bootloaders, 2 AC-controller firmwares, the MMU firmware, mini404 and
  the SVD files: 51 s, 1.3 GB.
- Failure at the first source archive:

```
  File "utils/bootstrap.py", line 223, in download_url
    response.raise_for_status()
requests.exceptions.HTTPError: 403 Client Error: Forbidden for url:
https://github.com/prusa3d/CrashDebug/archive/22acef8c6e248db2f04f65eebf4c2b470f4010c2.zip
```

Not downloaded at this point: `CrashDebug`, `esp8266-rtos-sdk`, `esp-idf`.

## Bootstrap, attempt 2: source archives fetched with git

No tracked file was changed. A wrapper imports `utils/bootstrap.py`,
replaces its `download_and_unzip` for `github.com/.../archive/...zip` URLs
with `git fetch --depth 1` + `git archive` of the same commit or tag, and
then calls the unmodified `bootstrap()`:

```python
#!/usr/bin/env python3
"""Run utils/bootstrap.py with GitHub source-archive downloads replaced by
shallow git fetches of the same commit.

Usage: .venv/bin/python bootstrap_via_git.py <repository root>
"""
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

repo_root = Path(sys.argv[1]).resolve()
sys.path.insert(0, str(repo_root / 'utils'))
import bootstrap  # noqa: E402

ARCHIVE_URL = re.compile(
    r'https://github\.com/([^/]+)/([^/]+)/archive/(refs/tags/)?(.+)\.zip$')
original_download_and_unzip = bootstrap.download_and_unzip


def download_and_unzip(url, directory):
    match = ARCHIVE_URL.match(url)
    if not match:
        return original_download_and_unzip(url=url, directory=directory)
    owner, repo, tag_prefix, ref = match.groups()
    print(f'Fetching {directory.name} with git ({owner}/{repo} {ref})',
          flush=True)
    shutil.rmtree(directory, ignore_errors=True)
    with tempfile.TemporaryDirectory(dir=bootstrap.dependencies_dir) as tmp:
        git = ['git', '-C', tmp]
        subprocess.run(git + ['init', '-q'], check=True)
        subprocess.run(git + [
            'fetch', '-q', '--depth', '1',
            f'https://github.com/{owner}/{repo}', (tag_prefix or '') + ref
        ],
                       check=True)
        directory.mkdir(parents=True)
        archive = subprocess.Popen(
            git + ['archive', '--format=tar', 'FETCH_HEAD'],
            stdout=subprocess.PIPE)
        subprocess.run(['tar', '-x', '-C', str(directory)],
                       stdin=archive.stdout,
                       check=True)
        if archive.wait() != 0:
            raise RuntimeError('git archive failed')


bootstrap.download_and_unzip = download_and_unzip
bootstrap.bootstrap(include_integration=False)
```

```bash
.venv/bin/python <scratch dir>/bootstrap_via_git.py "$PWD"
```

Result: **exit 0 after 55 s**. It fetched CrashDebug, the ESP8266 RTOS SDK
(+2 submodules) and ESP-IDF 5.0.3 (+6 submodules), ran both SDKs'
`install.sh` (xtensa toolchains from GitHub release assets, a Python
environment per SDK from PyPI), wrote `utils/debug/device_setup_overrides.cfg`
(git-ignored) and installed the git hook.

## Bootstrap totals

| Measure | Value |
|---|---|
| Wall time, both attempts | 197 s + 55 s = 252 s |
| `.dependencies/` (`du -sh`) | 2.4 GB |
| `.venv/` (`du -sh`) | 226 MB |
| Largest items | ARM toolchain 994 MB, ESP-IDF 804 MB (522 MB of it tools), ESP8266 SDK 384 MB (315 MB tools), cmake 165 MB, mini404 74 MB |

Tool versions that the bootstrap put under `.dependencies/` (each checked by
running it with `--version`): `arm-none-eabi-gcc` 13.3.1 20240614 (Arm GNU
Toolchain 13.3.Rel1), cmake 3.28.3, ninja 1.10.2, clang-format 16.0.0,
`qemu-system-buddy` from mini404 0.9.10. In `.venv`: Python 3.13.18,
prek 0.4.14.

Things worth knowing:

- `utils/bootstrap.py` imports `requests` in the interpreter that *starts*
  it, before the venv exists. The CI image installs `python3-requests` for
  that. Here `requests` happened to be importable for Python 3.13 but not
  for 3.12 (`python3.12 -c 'import requests'` fails with
  `ModuleNotFoundError`), so `python3.12 utils/bootstrap.py` would need
  `pip install requests` or `apt install python3-requests` first (not run).
- The venv inherits the Python that started the bootstrap. CI uses
  Ubuntu 24.04's `python3` (3.12); this machine's default is 3.13. All later
  stages use the 3.13 venv created here; deviations caused by that are
  noted where they occur.
- Bootstrap installs a `prek` pre-commit hook at `.git/hooks/pre-commit`
  (`prek install --overwrite -c .pre-commit-config.yaml`). From then on
  every `git commit` runs the format hooks on the staged files.
- The two ESP SDKs get their own Python environments under
  `.dependencies/<sdk>/.idf-tools/python_env/` (also Python 3.13 here).

## `pip install -r requirements.txt`

| Python | Command | Result | Wall time |
|---|---|---|---|
| 3.13.18 | run by `python3 utils/bootstrap.py` inside `.venv` (pip 23.3.2, cold cache) | works; `numpy==1.26.4` built from source (`numpy-1.26.4.tar.gz` → `numpy-1.26.4-cp313-cp313-linux_x86_64.whl`), needs gcc and the Python headers | 146 s |
| 3.12.3 | `python3.12 -m venv <scratch>/venv312 && <scratch>/venv312/bin/pip install --no-cache-dir -r requirements.txt` | works; wheels only, nothing compiled | 10 s |

Size of the 3.12 venv: 161 MB. `integration-test-requirements.txt` is
covered in stage 4.
