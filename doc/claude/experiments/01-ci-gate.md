# Stage 1: what CI enforces

Sources read: `utils/holly/build-pr.jenkins` (the only pipeline definition),
`utils/holly/Dockerfile`, `utils/holly/IntegrationTestsDockerfile`,
`.pre-commit-config.yaml`. The pipeline is a Jenkins declarative pipeline for
Prusa's build server ("Holly"). Nothing in this repository runs it for a
fork; this note is about what a change would have to pass upstream and how to
check the same things locally.

"PR build" below means `CHANGE_ID`/`CHANGE_TARGET` is set by Jenkins, "branch
build" means it is not.

## Pipeline stages

### 1. Prepare (three parallel stages)

| Stage | Runs when | What it does | Effect of failure |
|---|---|---|---|
| Create Git Tag | only when the repository URL is Prusa's private repository | tags `build/<version>` and pushes the tag | n/a for forks |
| Check Formatting | PR builds only | `prek run -c .pre-commit-config.yaml --source remotes/origin/<target> --origin HEAD --show-diff-on-failure --hook-stage manual` (hooks run only on files changed by the PR) | build marked FAILURE, later stages still run |
| Check Fixup Commits (Early Warning) | PR builds only | `git log --pretty=format:'%s' remotes/origin/<target>..HEAD --grep='^fixup!'` must print nothing | stage marked UNSTABLE only |

### 2. Build (matrix, one Docker container per configuration)

Every configuration is built with build type `release`. `yesboot` means
`--bootloader yes`, `noboot` means `--bootloader no`. No configuration uses
the `debug` build type or `--bootloader empty` (which is the default of
`utils/build.py`).

| Preset | PR build | Branch build | Bootloader | Comment in the pipeline |
|---|---|---|---|---|
| `coreonel_indx` | yes | yes | yes | |
| `coreone_indx` | yes | yes | yes | |
| `coreone` | yes | yes | yes | |
| `coreone_oak` | no | yes | yes | |
| `coreonel` | yes | yes | yes | |
| `mini-en-cs` | yes | yes | yes | "Always build for testing" |
| `mini-en-de`, `-es`, `-fr`, `-it`, `-ja`, `-uk` | no | yes | yes | |
| `mini-en-pl` | yes | yes | yes | "Always build due to largest memory requirement" |
| `mk3.5` | yes | yes | yes | |
| `mk4` | yes | yes | yes | |
| `xl` | yes | yes | yes | |
| `xl-burst` | no | yes | yes | |
| `xl-minimal` | no | yes | yes | "Test optional features on the most complex target" |
| `ix` | yes | yes | yes | |
| `slx-anfc` | yes | yes | yes | |
| `anfc-uart` | yes | yes | **no** | |
| `anfc` | yes | yes | yes | |

That is 13 configurations for a PR and 22 for a branch build. Notable
absences: the plain `mini` preset (all languages) is never built by CI, nor
are `mk4-burst`, `mini_big_flash`, `xl_dev_kit` or any of the sub-board
presets other than the three `anfc` ones (`xl-dwarf`, `xl-modularbed`,
`*-xbuddy_extension`, `*-indx_head`, `*-tool_offset_sensor`,
`xl-htcontroller`, `xl-can`, `ix-modularbed`) as presets of their own.
`mk4` `noboot` is commented out with "No integration tests anymore".

The exact command per configuration:

```bash
rm -rf build
python3 utils/build.py \
    --preset <preset> \
    --build-type release \
    --bootloader <yes|no> \
    --no-store-output \
    --skip-bootstrap \
    --version-suffix=<full suffix> \
    --version-suffix-short=<short suffix> \
    -DCUSTOM_COMPILE_OPTIONS:STRING="-Werror" \
    -DDEVELOPMENT_ITEMS_ENABLED:BOOL=<yes|no>
```

- **Warnings are errors in CI** (`-Werror`), unlike a plain local build.
- `DEVELOPMENT_ITEMS_ENABLED` is `no` on branches named `RELEASE-*`, `xl` or
  `mk4-release`, otherwise `yes` (so always `yes` for a PR).
- Version suffixes: PR `-DEV+<commit count>.PR<id>.B<build nr>`, release
  branch `-RC+<commit count>.B<build nr>`, other branches
  `-DEV+<commit count>.BRANCH-<name>.B<build nr>`; the short suffix is the
  part up to the commit count.
- Products archived: `build/products/*`.
- A failing configuration marks the build FAILURE; the rest still runs.

### 3. Tests (parallel stages)

| Stage | Runs when | What it does |
|---|---|---|
| Unit Tests | always | `python3 utils/build_tests.py --run -- --output-on-failure` (host build of all tests with the default build type, then all of ctest including the `slow` label). Archives `build/tests/Testing/Temporary/LastTest.log` on failure. |
| Marlin_server_types python module | **never** (commented out, "Temporarily disabled, see BFW-9038") | would `pip install` the binding and run its pytest |
| Integration Tests | **never** (the whole stage is commented out) | see below |
| Pre-release checks | always | `utils/generate-error-codes-report.py`, `utils/translations_and_fonts/generate-translations-report.sh`, `utils/generate-icon-parity-report.py`, each piped into `tee <name>.txt`; archives the three reports and `Prusa-Firmware-Buddy.pot` |
| Check Fixup Commits (Final) | PR builds only | same `git log --grep='^fixup!'` as above, but fails the build |

Integration tests, as the disabled code shows them: only on Prusa's private
repository and never for PRs (`should_run_integration_tests()`), 30 minute
timeout, in the image from `utils/holly/IntegrationTestsDockerfile`, on the
`mk4,release,noboot` firmware:

```bash
python3 utils/bootstrap.py --include-integration
python3 -m pytest tests/integration --firmware build/mk4_release_noboot/firmware.bin --log-cli-level info ...
```

Since both that stage and the `mk4` `noboot` build are commented out, **no
integration test runs anywhere in CI** at this commit.

The pre-release checks are reports, not gates: each script is the left side
of a pipe into `tee`, so its exit status is discarded.

### Size checks

There is no size-check stage, script or threshold in the pipeline. The only
size gate is the linker: each linker script declares the flash and RAM
regions (for example `FLASH_LENGTH = 895K` for the MINI with bootloader), so
a firmware that does not fit fails to link and therefore fails its Build
configuration. Headroom per preset is measured in stage 3.

## Format and lint hooks (`.pre-commit-config.yaml`)

| Hook | Source | Applies to |
|---|---|---|
| `cmake-format` | `cheshirekow/cmake-format-precommit` v0.6.13 | CMake files |
| `yapf` | `google/yapf` v0.43.0 | Python files |
| `clang-format` | local, `.dependencies/clang-format-16-83817c2f/clang-format -i -style=file` | C/C++ sources and headers |
| `generate-cmake-presets` | local, `python utils/build.py --generate-cmake-presets` | runs when any `.json` changes |
| `forbid-libc-assert` | local, `python utils/forbid_libc_assert.py` | always runs |
| `generate-log-components-overview` | local, `python utils/logging/generate_overview.py` | all files |
| `trailing-whitespace` (`--markdown-linebreak-ext=md`), `end-of-file-fixer`, `mixed-line-ending` | `pre-commit/pre-commit-hooks` v2.4.0 | all files |

A long `exclude` list keeps vendored code under `lib/` (and most of
`lib/Marlin` except the listed Prusa-maintained subdirectories) out of the
hooks. There is no clang-tidy, cppcheck or other static analysis step.

## CI images

### `utils/holly/Dockerfile` (formatting, builds, unit tests, reports)

- Base image: `ubuntu:24.04` (so `python3` is 3.12 and the host compiler is
  GCC 13).
- OS packages: `libjpeg-dev libusb-1.0-0 gettext git python3 python3-pip
  python3-venv python3-requests`.
- No compiler is listed. `apt-cache depends python3-pip` shows
  `Recommends: build-essential`, and the Dockerfile does not pass
  `--no-install-recommends`, so `gcc`/`g++`/`make` arrive as a recommended
  package of `python3-pip`. The unit tests depend on that.
- Then `python3 utils/bootstrap.py` in `/work`, which creates `/work/.venv`
  with `requirements.txt` and `/work/.dependencies` with everything in the
  bootstrap table (ARM GCC 13.3.rel1, cmake 3.28.3, ninja 1.10.2,
  clang-format 16, bootloaders, mini404, ESP SDKs, ...). Each job symlinks
  both directories into its workspace and activates the venv.
- Python packages (`requirements.txt`): `cbor2~=5.7.1 ecdsa~=0.18 gcovr~=7.2
  jsonschema~=4.25.1 ndeflib~=0.3.3 numpy==1.26.4 nunavut~=2.3 pillow~=12.3
  pip~=23.0 polib~=1.2 prek~=0.4.5 pytest~=7.3.2 pyyaml~=6.0 qoi~=0.5
  referencing~=0.37.0 requests==2.32.3 setuptools~=70.1.0
  simple_parsing~=0.1.7 tree-sitter~=0.23 tree-sitter-cpp~=0.23
  virtualenv~=20.26`.

### `utils/holly/IntegrationTestsDockerfile` (unused at this commit)

- Base image: `ubuntu:22.04`.
- OS packages: `curl python3 python3-venv python3-dev python3-requests
  libncurses5 g++-multilib git libglib2.0-dev libfdt-dev libpixman-1-dev
  zlib1g-dev ninja-build libaio-dev libbluetooth-dev libcapstone-dev
  libbrlapi-dev libbz2-dev libcap-ng-dev libcurl4-gnutls-dev libgtk-3-dev
  libibverbs-dev libncurses5-dev libnuma-dev librbd-dev librdmacm-dev
  libsasl2-dev libsdl2-dev libseccomp-dev libsnappy-dev libssh-dev
  libvde-dev libvdeplug-dev libvte-2.91-dev libxen-dev liblzo2-dev valgrind
  xfslibs-dev libgl1 libglu1 freeglut3 libglew2.2 libepoxy-dev
  libusb-1.0-0-dev` (the QEMU-style list is what the simulator binary links
  against).
- Then `python3 utils/bootstrap.py --include-integration`, which adds
  `integration-test-requirements.txt`: `aiohttp~=3.8 click~=8.1.3
  easyocr~=1.7 pytest-asyncio~=0.21`.

## What was run locally for this stage

| Command | Result |
|---|---|
| `prek run -c .pre-commit-config.yaml --source remotes/origin/master --origin HEAD --show-diff-on-failure --hook-stage manual` (venv active, on this branch) | exit 0, 0.8 s; hooks that had files to check passed. The hook environments had been set up by the first `git commit` after bootstrap (4 s including set-up). |
| `git log --pretty=format:'%s' remotes/origin/master..HEAD --grep='^fixup!'` | prints nothing |
| `utils/generate-error-codes-report.py` | exit 0, 0.2 s; reports error codes `XX146`, `XX148`, `XX149` as "not approved" on unmodified `master` |
| `utils/generate-icon-parity-report.py` | exit 0, 0.1 s |
| `utils/translations_and_fonts/generate-translations-report.sh` | first exit 127: `xgettext: command not found`; after `apt-get install -y gettext` exit 1 after 2.3 s (details below) |
| The 13 PR builds with `-Werror` | not run in this stage; stage 3 builds presets without `-Werror` and records any exception |
| `python3 utils/build_tests.py --run -- --output-on-failure` | see stage 2 |

Translations report details: `generate_pot.sh` needs `xgettext`, the report
needs `msgmerge` and `msgattrib` (package `gettext`). With gettext installed
the script regenerates `src/lang/po/Prusa-Firmware-Buddy.pot` and then stops
with exit 1 in its `find ... | while read` loop, because `grep ^msgid`
returns 1 for the first `.po` file that has no untranslated string (here
`pl`) and the script runs under `set -e`. When it stops there it leaves the
tracked `.pot` file modified (restore with
`git checkout -- src/lang/po/Prusa-Firmware-Buddy.pot`); when gettext is
missing it leaves `tmp_error_headers/` and `tmp_filelist` in the repository
root. CI does not notice either, because of the pipe into `tee` and because
the workspace is deleted.

## To reproduce the PR gate locally, run these commands

```bash
python3 utils/bootstrap.py            # once; see stage 0
. .venv/bin/activate

# 1. formatting (target branch is the branch the PR would merge into)
prek run -c .pre-commit-config.yaml --source remotes/origin/master \
    --origin HEAD --show-diff-on-failure --hook-stage manual

# 2. no fixup commits: must print nothing
git log --pretty=format:'%s' remotes/origin/master..HEAD --grep='^fixup!'

# 3. the 13 PR build configurations, warnings as errors
for preset in coreonel_indx coreone_indx coreone coreonel mini-en-cs \
              mini-en-pl mk3.5 mk4 xl ix slx-anfc anfc; do
    python3 utils/build.py --preset $preset --build-type release \
        --bootloader yes --skip-bootstrap \
        -DCUSTOM_COMPILE_OPTIONS:STRING="-Werror" \
        -DDEVELOPMENT_ITEMS_ENABLED:BOOL=yes
done
python3 utils/build.py --preset anfc-uart --build-type release \
    --bootloader no --skip-bootstrap \
    -DCUSTOM_COMPILE_OPTIONS:STRING="-Werror" \
    -DDEVELOPMENT_ITEMS_ENABLED:BOOL=yes

# 4. unit tests
python3 utils/build_tests.py --run -- --output-on-failure

# 5. reports (informational, need the gettext package)
utils/generate-error-codes-report.py
utils/translations_and_fonts/generate-translations-report.sh
utils/generate-icon-parity-report.py
```

Steps 1, 2 and 5 were run as shown. Step 3 as a whole loop with `-Werror`
was not run; stage 3 records which of these presets were built and how long
one build takes, and stage 2 covers step 4. CI additionally passes
`--no-store-output` and version suffixes and starts each configuration from
`rm -rf build`; these do not change what is compiled.
