===================
Build DFTracer
===================

This section describes how to build DFTracer.

There are three build options:

- build DFTracer with pip (recommended),
- build DFTracer with Spack, and
- build DFTracer with cmake

----------

------------------------------------------
Create Python environment (Recommended)
------------------------------------------
Creating a Python environment is the cleanest way to install DFTracer.

.. code-block:: Bash

    python -m venv <PYTHON-VENV-PATH>
    source <PYTHON-VENV-PATH>/bin/activate


------------------------------------------
Build DFTracer with pip (Recommended)
------------------------------------------

Users can easily install DFTracer using pip. This is the way most python packages are installed.
This method would work for both native python environments and conda environments.


Install DFTracer
*******************************

From PyPI (Recommended)
************************

.. code-block:: Bash

    pip install dftracer

.. attention::

    For pip installations, all libraries will be present within the site-packages/dftracer/lib.
    This enables clean management of pip installation and uninstallations.

From source
************

.. code-block:: Bash

    git clone git@github.com:LLNL/dftracer.git
    cd dftracer
    # You can skip this for installing the dev branch.
    # for latest stable version use master branch.
    git checkout tags/<Release> -b <Release>
    pip install .

From Github
************

.. code-block:: Bash

  DFT_VERSION=v1.0.4
  pip install git+https://github.com/LLNL/dftracer.git@${DFT_VERSION}

.. attention::

    For pip installations, all libraries will be present within the site-packages/dftracer/lib.
    This enables clean management of pip installation and uninstallations.

-----------------------------------------
Build DFTracer with Spack
-----------------------------------------


One may install DFTracer with Spack_.
If you already have Spack, make sure you have the latest release.
If you use a clone of the Spack develop branch, be sure to pull the latest changes.

.. _build-label:

Install Spack
*************
.. code-block:: Bash

    $ git clone https://github.com/spack/spack
    $ # create a packages.yaml specific to your machine
    $ . spack/share/spack/setup-env.sh

Use `Spack's shell support`_ to add Spack to your ``PATH`` and enable use of the
``spack`` command.

Build and Install DFTracer
*******************************

.. code-block:: Bash

    $ spack install py-pydftracer
    $ spack load py-pydftracer

If the most recent changes on the development branch ('dev') of DFTracer are
desired, then do ``spack install py-pydftracer@develop``.

.. attention::

    The initial install could take a while as Spack will install build
    dependencies (autoconf, automake, m4, libtool, and pkg-config) as well as
    any dependencies of dependencies (cmake, perl, etc.) if you don't already
    have these dependencies installed through Spack or haven't told Spack where
    they are locally installed on your system (i.e., through a custom
    packages.yaml_).
    Run ``spack spec -I py-dftracer-py`` before installing to see what Spack is going
    to do.

----------

------------------------------
Build DFTracer with CMake
------------------------------

Download the latest DFTracer release from the Releases_ page or clone the develop
branch ('develop') from the DFTracer repository
`https://github.com/LLNL/dftracer <https://github.com/LLNL/dftracer>`_.

---------------
Build Variables
---------------

.. table:: section - main build settings using env variables or cmake flags
   :widths: auto

   ================================ ======  ===========================================================================
   Environment Variable             Type    Description
   ================================ ======  ===========================================================================
   DFTRACER_BUILD_TYPE              STRING  Sets the build type for DFTRACER (default Release). Values are Debug or Release
   DFTRACER_ENABLE_FTRACING         BOOL    Enables function tracing (default OFF).
   DFTRACER_ENABLE_HIP_TRACING      BOOL    Enables AMD GPU tracing (default OFF).
   DFTRACER_ENABLE_MPI              BOOL    Enables MPI Rank (default ON).
   DFTRACER_DISABLE_HWLOC           BOOL    Disables HWLOC (default ON).
   DFTRACER_PYTHON_EXE              STRING  Sets path to python executable. Only Cmake.
   DFTRACER_PYTHON_SITE             STRING  Sets path to python site-packages. Only Cmake.
   DFTRACER_BUILD_PYTHON_BINDINGS   STRING  Enable python bindings for DFTracer. Only Cmake.
   DFTRACER_WRITER_TYPE             STRING  Sets the writer backend (default STDIO). Values are STDIO, MOFKA, or CHRONOLOG. Only Cmake.
   ================================ ======  ===========================================================================

These build variables can be set with cmake as ``-DDISABLE_HWLOC=OFF`` or as environment variables ``export DFTRACER_DISABLE_HWLOC=OFF``

-------------------
Writer Backends
-------------------

DFTracer supports multiple writer backends that can be selected at compile time using the ``DFTRACER_WRITER_TYPE`` CMake option:

**STDIO Writer (default)**
  Writes trace data to local files. This is the default backend and requires no additional dependencies.

**MOFKA Writer**
  Writes trace data to Mofka event streaming service. Requires the Mofka library to be installed.
  
  Runtime configuration (via environment variables):
  
  - ``DFTRACER_MOFKA_GROUP_FILE``: Path to the Mofka group file (required)
  - ``DFTRACER_MOFKA_TOPIC_NAME``: Name of the Mofka topic (default: "dftracer_events")

**CHRONOLOG Writer**
  Writes trace data to ChronoLog distributed event logging system. Requires the ChronoLog client library. ChronoLog's public headers (e.g. ``ClientConfiguration.h``) include ``spdlog``; ChronoLog does not install spdlog itself—it is provided by the Spack environment used to build ChronoLog (see ChronoLog's ``spack.yaml``). When building DFTracer with the CHRONOLOG writer, use the **same** Spack environment: activate it (e.g. ``spack env activate /path/to/ChronoLog``) or set ``CHRONOLOG_SPACK_ENV`` and run ``script/build-with-chronolog.sh`` so ``CMAKE_PREFIX_PATH`` includes the view and spdlog is found.
  
  Runtime configuration (via environment variables):
  
  - ``DFTRACER_CHRONOLOG_PROTOCOL``: Transport protocol (default: "ofi+sockets")
  - ``DFTRACER_CHRONOLOG_HOST``: ChronoVisor host address (default: "127.0.0.1")
  - ``DFTRACER_CHRONOLOG_PORT``: ChronoVisor port (default: 5555)
  - ``DFTRACER_CHRONOLOG_PROVIDER_ID``: Provider ID (default: 55)
  - ``DFTRACER_CHRONOLOG_CHRONICLE_NAME``: Chronicle name (default: "dftracer_chronicle")
  - ``DFTRACER_CHRONOLOG_STORY_NAME``: Story name (default: "dftracer_story")

  ChronoLog does not use pub/sub: trace data is read by polling. When building with the CHRONOLOG writer, the **dftracer_chronolog_reader** tool is also built. It connects in reader mode (portal + query service) and in a loop calls ``Client::ReplayStory()`` to retrieve events and write them to a file or stdout. The query service defaults to port 5557 (override with ``DFTRACER_CHRONOLOG_QUERY_HOST`` / ``DFTRACER_CHRONOLOG_QUERY_PORT``). Use it to drain trace data into a ``.pfw`` file for DFAnalyzer or Perfetto. Example: ``dftracer_chronolog_reader --once --output trace.pfw`` (one-shot) or ``dftracer_chronolog_reader --output trace.pfw --poll-interval 2`` (continuous). Tests use the reader in ``--once`` mode and verify event count.

  **Testing in Docker (ChronoVisor already running):** From inside the container, set ``CHRONOLOG_INSTALL_DIR`` to the ChronoLog install prefix, build DFTracer with ``script/build-with-chronolog.sh``, then run the write+read smoke test: ``bash script/run-chronolog-write-read-test.sh [install_prefix]``. If ChronoVisor is in another container, set ``DFTRACER_CHRONOLOG_HOST`` to that host (e.g. the service name). The script runs the tracer, drains events with the reader into ``/tmp/dftracer_chronolog_events.pfw``, and prints the event count.

To build with a specific writer backend:

.. code-block:: Bash

    # Build with STDIO writer (default)
    cmake . -B build -DCMAKE_INSTALL_PREFIX=<install-path>
    
    # Build with MOFKA writer
    cmake . -B build -DCMAKE_INSTALL_PREFIX=<install-path> -DDFTRACER_WRITER_TYPE=MOFKA
    
    # Build with CHRONOLOG writer
    cmake . -B build -DCMAKE_INSTALL_PREFIX=<install-path> -DDFTRACER_WRITER_TYPE=CHRONOLOG -DCHRONOLOG_INSTALL_DIR=<chronolog-install-path>

For the CHRONOLOG writer, you can either:

1. Set ``CHRONOLOG_INSTALL_DIR`` to point to your ChronoLog installation directory, or
2. Ensure ChronoLog is findable via CMake's ``find_package()`` by adding it to ``CMAKE_PREFIX_PATH``

  spdlog (required by ChronoLog headers) is found from the ChronoLog Spack env view (when that env is active or ``CHRONOLOG_SPACK_ENV`` is set), from ``CMAKE_PREFIX_PATH``, or from ``SPDLOG_INSTALL_DIR``. The script ``script/build-with-chronolog.sh`` can source the ChronoLog Spack env when ``CHRONOLOG_SPACK_ENV`` is set so the same view used to build ChronoLog is used when building DFTracer.

Build DFTracer Dependencies
********************************

The main dependencies DFTracer are
1. cpp-logger : `https://github.com/hariharan-devarajan/cpp-logger.git <https://github.com/hariharan-devarajan/cpp-logger.git>`_ version: 0.0.1
2. gotcha: `https://github.com/LLNL/GOTCHA.git <https://github.com/LLNL/GOTCHA.git>`_ version: develop
3. brahma: `https://github.com/hariharan-devarajan/brahma.git <https://github.com/hariharan-devarajan/brahma.git>`_ version: 0.0.1

These dependencies can be either installed using spack or through cmake from respective respositories.

.. code-block:: Bash
    
    cmake . -B build -DCMAKE_INSTALL_PREFIX=<where you want to install DFTracer>
    cmake --build build
    cmake --install build

-----------

.. explicit external hyperlink targets

.. _Releases: https://github.com/LLNL/dftracer/releases
.. _Spack: https://github.com/spack/spack
.. _Spack's shell support: https://spack.readthedocs.io/en/latest/getting_started.html#add-spack-to-the-shell
.. _packages.yaml: https://spack.readthedocs.io/en/latest/build_settings.html#external-packages
