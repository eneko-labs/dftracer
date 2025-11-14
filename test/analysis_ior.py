import pandas as pd
import numpy as np

df = pd.read_csv('cases.csv')
unique_modes = df['mode'].unique().tolist()
print("access_type,mode,baseline_avg,baseline_std,mode_avg,mode_std,overhead_percent")
for access_type in ['write', 'read']:
    base = df[(df['access'] == access_type) & (df['mode'] == 'none')]
    base_avg = base['bw(MiB/s)'].mean()
    base_std = base['bw(MiB/s)'].std()
    for mode in unique_modes:
        mode_df = df[(df['access'] == access_type) & (df['mode'] == mode)]
        mode_avg = mode_df['bw(MiB/s)'].mean()
        mode_std = mode_df['bw(MiB/s)'].std()
        overhead = ((mode_avg - base_avg) / base_avg) * 100 if base_avg != 0 else 0
        print(f"{access_type},{mode},{base_avg:.2f},{base_std:.2f},{mode_avg:.2f},{mode_std:.2f},{overhead:.2f}")