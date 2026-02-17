# Python tests

On systems where the system Python is externally managed (PEP 668), use a virtual environment.

## Prerequisites

**Debian/Ubuntu:** `python3 -m venv` needs the venv package. If you see "ensurepip is not available", run:

```bash
sudo apt install python3.12-venv
```

(Use your Python minor version, e.g. `python3.11-venv` if you have 3.11.)

## Setup (from repo root)

If a previous venv creation failed, remove it first: `rm -rf venv`

```bash
# Create venv (once)
python3 -m venv venv
# Install test deps and the dftracer package (provides dftracer.python, pydftracer)
./venv/bin/pip install -r test/py/requirements.txt
./venv/bin/pip install -e .
```

## Run tests

Point CMake at the venv’s Python, build, install the built `.so` into the venv, then run CTest:

```bash
cd build
cmake -DDFTRACER_PYTHON_EXE=../venv/bin/python3 ..
make
make install
ctest --output-on-failure
```

`make install` symlinks the built Python extension into the venv’s site-packages so tests can import `dftracer`.

If you pass `../venv/bin/python` and it does not exist, CMake will use `../venv/bin/python3` automatically. Paths are resolved to absolute so CTest finds the executable.

## Without a venv (not recommended)

If you cannot install `python3-venv`, you can use system Python and install packages with:

```bash
pip install --break-system-packages -r test/py/requirements.txt
```

Then configure **without** `-DDFTRACER_PYTHON_EXE` so CMake uses the default `python3`.

Required for tests: `numpy`, `h5py`, `Pillow` (and optionally others in `requirements.txt`).
