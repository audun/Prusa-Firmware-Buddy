#!/usr/bin/env python3
"""Run the Buddy simulator headless and drive it through its script console.

Usage: sim_probe.py <work dir> <seconds> [--shots N] [--at SECONDS CMD]...
                    -- <qemu-system-buddy> <qemu arguments>...

QEMU is started in <work dir> with the given arguments plus a script console
on a free TCP port. Everything QEMU prints is echoed with a timestamp. A
screenshot is saved to <work dir>/shot-NNN.png every N seconds (default 10)
and every `--at SECONDS CMD` line is sent to the script console at that time
(for example `--at 30 'encoder-input::Push()'`). The literal `{work}` in the
QEMU arguments and commands is replaced by the absolute work directory.
"""
import argparse
import asyncio
import socket
import sys
import time
from pathlib import Path


def free_port():
    with socket.socket() as s:
        s.bind(('', 0))
        return s.getsockname()[1]


async def pump(stream, tag, start):
    while not stream.at_eof():
        line = (await stream.readline()).decode('utf-8', 'replace').rstrip()
        if line and 'Bus unhandled low duration' not in line:
            print(f'{time.time() - start:7.1f} {tag} {line}', flush=True)


async def main():
    split = sys.argv.index('--')
    parser = argparse.ArgumentParser()
    parser.add_argument('workdir', type=Path)
    parser.add_argument('seconds', type=float)
    parser.add_argument('--shots', type=float, default=10)
    parser.add_argument('--at', nargs=2, action='append', default=[])
    args = parser.parse_args(sys.argv[1:split])
    work = args.workdir.resolve()
    work.mkdir(parents=True, exist_ok=True)
    port = free_port()
    command = [
        arg.replace('{work}', str(work)) for arg in sys.argv[split + 1:]
    ]
    command += [
        '-chardev',
        f'socket,id=p404-scriptcon,port={port},host=localhost,server=on',
        '-global', 'p404-scriptcon.no_echo=true'
    ]
    schedule = sorted(
        (float(t), c.replace('{work}', str(work))) for t, c in args.at)
    start = time.time()
    process = await asyncio.create_subprocess_exec(
        *command,
        cwd=work,
        stdin=asyncio.subprocess.DEVNULL,
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE)
    pumps = [
        asyncio.ensure_future(pump(process.stdout, 'out', start)),
        asyncio.ensure_future(pump(process.stderr, 'err', start))
    ]
    reader = writer = None
    while writer is None and process.returncode is None:
        try:
            reader, writer = await asyncio.open_connection('localhost', port)
        except OSError:
            await asyncio.sleep(0.5)

    async def replies():
        while reader and not reader.at_eof():
            line = (await reader.readline()).decode('utf-8', 'replace')
            if line.strip():
                print(f'{time.time() - start:7.1f} <<< {line.rstrip()}',
                      flush=True)

    pumps.append(asyncio.ensure_future(replies()))

    async def script(line):
        print(f'{time.time() - start:7.1f} >>> {line}', flush=True)
        writer.write(line.encode() + b'\n')
        await writer.drain()

    shot = 0
    while process.returncode is None and time.time() - start < args.seconds:
        await asyncio.sleep(0.5)
        now = time.time() - start
        if process.returncode is not None:
            break
        while schedule and schedule[0][0] <= now:
            await script(schedule.pop(0)[1])
            await asyncio.sleep(0.3)
        if args.shots and now // args.shots > shot:
            shot = int(now // args.shots)
            await script(
                f'generic-spi-display::Screenshot({work}/shot-{shot:03d}.png)')
    if process.returncode is None:
        print(f'{time.time() - start:7.1f} still running, terminating',
              flush=True)
        process.terminate()
    await process.wait()
    await asyncio.wait(pumps, timeout=3)
    for task in pumps:
        task.cancel()
    print(f'{time.time() - start:7.1f} exit code {process.returncode}',
          flush=True)


asyncio.run(main())
