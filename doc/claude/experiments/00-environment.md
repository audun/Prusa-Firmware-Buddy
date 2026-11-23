# Stage 0: environment and bootstrap feasibility

Status: **preliminary** (environment recorded, bootstrap still running when
this version was committed; the file is updated when the bootstrap finishes).

Starting point: branch `master` at `b1043d68b`, `version.txt` = `8.0.2`.

## Machine

Commands: `uname -srm`, `. /etc/os-release; echo $PRETTY_NAME`, `nproc`,
`free -h`, `df -h .`, `lscpu`, `<tool> --version`.

| Item | Value |
|---|---|
| Kernel | Linux 6.18.44 x86_64 (virtual machine / container) |
| Distribution | Ubuntu 24.04.5 LTS |
| CPUs | 4 (Intel Xeon @ 2.10 GHz, 1 thread per core) |
| RAM | 15 GiB, no swap |
| Free disk | 60 GiB |
| python3 | 3.13.18 (default `python3`); 3.12 and 3.11 also installed |
| gcc / g++ | 13.3.0 (Ubuntu 13.3.0-6ubuntu2~24.04.1) |
| clang | 18.1.3 |
| cmake | 3.28.3 |
| ninja | 1.11.1 |
| make | GNU Make 4.3 |
| git | 2.43.0 |
| ccache | not installed |
| pip | 24.0 |

Network: outbound HTTPS only, through a filtering proxy that re-terminates
TLS (tools must trust an extra CA bundle; `requests`, `pip` and `curl` are
pre-configured for it through the usual environment variables).

## Download hosts used by `utils/bootstrap.py`

Command (one `curl -sIL -o /dev/null -w '%{http_code}' <url>` per host, URLs
taken from the `dependencies` table in `utils/bootstrap.py`):

| Host / URL shape | Used for | HEAD result |
|---|---|---|
| `github.com/<owner>/<repo>/releases/download/...` | ninja, cmake, mini404, MMU firmware | 200 |
| `developer.arm.com` | `gcc-arm-none-eabi` 13.3.rel1 | 200 |
| `prusa-buddy-firmware-dependencies.s3.eu-central-1.amazonaws.com` | clang-format, bootloaders, AC controller firmware | 200 |
| `raw.githubusercontent.com` | CMSIS SVD files | 200 |
| `dl.espressif.com` | ESP SDK tool installers | 200 |
| `pypi.org` | pip packages | 200 |
| `github.com/<owner>/<repo>/archive/<ref>.zip` (and `codeload.github.com`) | CrashDebug, ESP8266 RTOS SDK, ESP-IDF and their submodules | **403** |

The 403 is a policy of this sandbox, not of GitHub: source-archive downloads
are refused for repositories that are not attached to the session (release
assets and `git` protocol reads of public repositories work). On an ordinary
Linux machine these URLs are public.

## Bootstrap

Result pending, see the updated version of this file.
