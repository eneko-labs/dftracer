# Python tests

On systems where the system Python is externally managed (PEP 668), use a virtual environment.

From the repo root:

```bash
# Create venv (once)
python3 -m venv venv
# Activate and install test deps
. venv/bin/activate   # or on Windows: venv\Scripts\activate
pip install -r test/py/requirements.txt
```

Run CTest using the venv’s Python so the tests see the installed packages:

```bash
cd build
cmake -DDFTRACER_PYTHON_EXE=../venv/bin/python ..
make
ctest --output-on-failure
```

If the venv is activated, you can instead pass the current Python:  
`cmake -DDFTRACER_PYTHON_EXE=$(which python) ..`

Required for tests: `numpy`, `h5py`, `Pillow` (and optionally others in `requirements.txt`).
