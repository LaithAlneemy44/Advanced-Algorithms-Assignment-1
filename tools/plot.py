"""Plot swiss_bench_maps results.

One chart per workload: nanoseconds per operation against the number of keys,
one line per table. With --benchmark_repetitions, points are means and error
bars are one standard deviation across repetitions.

Benchmark names are <workload>/<table>/<n>, and each iteration performs n
operations, so ns per operation = real_time / n.

Usage:
    uv run --with matplotlib python tools/plot.py RESULTS.json OUTPUT_DIR
"""

import collections
import json
import pathlib
import sys

import matplotlib

matplotlib.use("Agg")  # render to files, no window
import matplotlib.pyplot as plt


def load(path):
    """Returns {workload: {table: {n: (mean_ns, stddev_ns)}}}."""
    data = json.loads(pathlib.Path(path).read_text())
    runs = data["benchmarks"]
    have_aggregates = any(b.get("run_type") == "aggregate" for b in runs)

    stats = collections.defaultdict(lambda: collections.defaultdict(dict))
    for b in runs:
        workload, table, n = b["run_name"].split("/")[:3]
        n = int(n)
        assert b["time_unit"] == "ns", b["name"]
        per_op = b["real_time"] / n
        mean, std = stats[workload][table].get(n, (None, 0.0))
        if have_aggregates:
            if b.get("aggregate_name") == "mean":
                mean = per_op
            elif b.get("aggregate_name") == "stddev":
                std = per_op
        elif b.get("run_type") == "iteration":
            mean = per_op
        stats[workload][table][n] = (mean, std)
    return stats


def plot(stats, out_dir):
    out_dir = pathlib.Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    for workload, tables in sorted(stats.items()):
        fig, ax = plt.subplots(figsize=(7, 4.5))
        for table, points in sorted(tables.items()):
            ns = sorted(points)
            means = [points[n][0] for n in ns]
            stds = [points[n][1] for n in ns]
            ax.errorbar(ns, means, yerr=stds, marker="o", markersize=4, capsize=3, label=table)
        ax.set_xscale("log", base=2)
        ax.set_yscale("log")
        ax.set_xlabel("keys in table")
        ax.set_ylabel("ns per operation")
        ax.set_title(workload)
        ax.grid(True, which="both", alpha=0.3)
        ax.legend()
        fig.tight_layout()
        fig.savefig(out_dir / f"{workload}.png", dpi=120)
        plt.close(fig)
        print(f"wrote {out_dir / (workload + '.png')}")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    plot(load(sys.argv[1]), sys.argv[2])
