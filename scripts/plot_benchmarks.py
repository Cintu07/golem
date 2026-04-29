#!/usr/bin/env python3
"""
Read CSV benchmark output files and produce one PNG bar chart per file.

Usage:
    python3 plot_benchmarks.py results/bench_vector.csv results/bench_optional.csv ...

Each CSV row must be: benchmark_name,golem_ns,std_ns
Output: one PNG next to each CSV (e.g. bench_vector.png).
"""

import csv
import pathlib
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np


def ns_to_label(ns: float) -> str:
    if ns >= 1e9:
        return f"{ns/1e9:.2f}s"
    if ns >= 1e6:
        return f"{ns/1e6:.2f}ms"
    if ns >= 1e3:
        return f"{ns/1e3:.1f}µs"
    return f"{ns:.0f}ns"


def plot_csv(path: str) -> None:
    rows = []
    with open(path, newline="") as f:
        for r in csv.reader(f):
            if len(r) == 3:
                try:
                    rows.append((r[0].strip(), float(r[1]), float(r[2])))
                except ValueError:
                    pass

    if not rows:
        print(f"  no data in {path}, skipping")
        return

    names      = [r[0] for r in rows]
    golem_vals = [r[1] / 1e6 for r in rows]   # nanoseconds -> milliseconds
    std_vals   = [r[2] / 1e6 for r in rows]

    x     = np.arange(len(names))
    width = 0.35

    fig, ax = plt.subplots(figsize=(max(6, len(names) * 2.0), 5))

    bars_g = ax.bar(x - width / 2, golem_vals, width,
                    label="golem", color="#4a90d9", zorder=3)
    bars_s = ax.bar(x + width / 2, std_vals,   width,
                    label="std",   color="#aaaaaa", zorder=3)

    # value labels on top of each bar
    for bar, ns_val in zip(bars_g, [r[1] for r in rows]):
        ax.text(bar.get_x() + bar.get_width() / 2, bar.get_height(),
                ns_to_label(ns_val), ha="center", va="bottom", fontsize=8)
    for bar, ns_val in zip(bars_s, [r[2] for r in rows]):
        ax.text(bar.get_x() + bar.get_width() / 2, bar.get_height(),
                ns_to_label(ns_val), ha="center", va="bottom", fontsize=8)

    ax.set_ylabel("time (ms, lower is better)")
    ax.set_title(pathlib.Path(path).stem.replace("_", " "), pad=28)
    ax.set_xticks(x)
    ax.set_xticklabels(names, rotation=20, ha="right")
    ax.legend(loc="upper center", bbox_to_anchor=(0.5, 1.12), ncol=2)
    current_top = ax.get_ylim()[1]
    ax.set_ylim(top=current_top * 1.22)
    ax.yaxis.grid(True, linestyle="--", alpha=0.5, zorder=0)
    ax.set_axisbelow(True)

    fig.tight_layout()

    out = str(pathlib.Path(path).with_suffix(".png"))
    fig.savefig(out, dpi=130)
    plt.close(fig)
    print(f"  wrote {out}")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("usage: plot_benchmarks.py file1.csv [file2.csv ...]")
        sys.exit(1)
    for p in sys.argv[1:]:
        print(f"plotting {p}")
        plot_csv(p)
