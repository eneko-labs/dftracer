from dftracer.dynamo import dft_fn
from dftracer.logger import dftracer
import torch
import os

# Delete log file if exists
if os.path.exists("dynamo.pfw"):
    os.remove("dynamo.pfw")

df_logger = dftracer.initialize_log(f"dynamo.pfw", None, -1)
dyn = dft_fn(name="dynamo", enabled=True)


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


if __name__ == "__main__":
    model = SimpleModel()
    t_model: torch.nn.Module = model  # type: ignore
    # Create random input
    sample = torch.randn(1, 3, 32, 32)
    print(t_model(sample))
    df_logger.finalize()
    with open("dynamo.pfw", "rb") as f:
        data = f.read()
        var = b'"cat":"dynamo"' in data
        assert var
        print("DFTracer model tracing is enabled and working")
