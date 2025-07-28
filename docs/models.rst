======================
Model tracing
======================

DFTracer can trace the execution of PyTorch models, providing insights into the model's performance and behavior during training.


-------------------------
DFTracer Python APIs
-------------------------

This section describes how to use DFTracer for profiling pytorch applications.

-----

Installing DFTracer with model tracing
****************************************

Install DFTracer with model tracing

.. code-block:: bash
    pip install pydftracer[dynamo]

Function decorator style profiling
****************************************

With python applications, after initializing the dftracer module developers can use decorator provided within dftracer.dynamo to tag functions that need to be profiled.
To use the function decorators, they can be initialized in place or globally to reuse within many functions.

.. code-block:: python

    dftracer.dynamo import dft_fn as dyn_fn
    from dftracer.logger import dftracer 

    log_inst = dftracer.initialize_log(logfile=None, data_dir=None, process_id=-1)

    dyn = dyn_fn("dynamo")

    class SimpleModel(torch.nn.Module):
      def __init__(self):
          super(SimpleModel, self).__init__()
          self.conv = torch.nn.Conv2d(3, 16, 3, 1)
          self.fc = torch.nn.Linear(16 * 15 * 15, 10)

      @dyn.compile
      def forward(self, x):
          x = self.conv(x)
          x = torch.nn.functional.relu(x)
          x = torch.nn.functional.max_pool2d(x, 2)
          x = x.view(x.size(0), -1)
          x = self.fc(x)
          return x


Model profiling
****************************************

.. code-block:: python
    from dftracer.dynamo import dft_fn as dyn_fn
    from dftracer.logger import dftracer
    log_inst = dftracer.initialize_log(logfile=None, data_dir=None, process_id=-1)
    dyn = dyn_fn("dynamo")

    class SimpleModel(torch.nn.Module):
      def __init__(self):
          super(SimpleModel, self).__init__()
          self.conv = torch.nn.Conv2d(3, 16, 3, 1)
          self.fc = torch.nn.Linear(16 * 15 * 15, 10)

      def forward(self, x):
          x = self.conv(x)
          x = torch.nn.functional.relu(x)
          x = torch.nn.functional.max_pool2d(x, 2)
          x = x.view(x.size(0), -1)
          x = self.fc(x)
          return x


    model = SimpleModel()
    model = dyn(model)

.. _`PyTorch Dynamo`: https://docs.pytorch.org/docs/stable/torch.compiler_dynamo_overview.html