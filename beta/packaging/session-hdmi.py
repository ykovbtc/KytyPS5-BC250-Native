"""Switch the active TV for a Minimal Mode game. Return its previous mode for restoration."""
import json
from pathlib import Path
import subprocess
import sys


RANDR = Path(__file__).resolve().parent / "system/session-runtime/bin/wlr-randr"


def mode_name(mode):
    return f"{mode['width']}x{mode['height']}@{mode['refresh']:.6f}Hz"


def select_mode(outputs, size):
    active = [head for head in outputs if head["enabled"]]
    if len(active) != 1:
        active = [head for head in active if head["name"].startswith("HDMI-")]
    if len(active) != 1:
        raise ValueError("Could not identify a single active TV output.")
    head = active[0]
    modes = head["modes"]
    current = next((mode for mode in modes if mode["current"]), None)
    if current is None:
        raise ValueError(f"Could not read the current mode of {head['name']}.")
    choices = [mode for mode in modes if f"{mode['width']}x{mode['height']}" == size]
    if not choices:
        raise ValueError(f"{head['name']} does not advertise {size}. Choose another HDMI output resolution.")
    target = min(choices, key=lambda mode: abs(mode["refresh"] - current["refresh"]))
    return head["name"], mode_name(current), mode_name(target)


def switch(output, mode):
    subprocess.run([RANDR, "--output", output, "--mode", mode], check=True, timeout=8)


def main(args):
    if len(args) == 2:
        switch(*args)
        return
    size, = args
    outputs = json.loads(subprocess.check_output([RANDR, "--json"], timeout=8))
    output, old, new = select_mode(outputs, size)
    if old != new:
        switch(output, new)
        print(json.dumps([output, old]))


if __name__ == "__main__":
    try:
        main(sys.argv[1:])
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        raise SystemExit(f"HDMI output: {error}")
