# Turns results.csv into the figures used in the report.
#
#   usage: python3 plot_results.py

import csv
import os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

RESULTS = "results.csv"
FIGURE_DIRECTORY = "figures"

SURFACE = "#fcfcfb"
TEXT_PRIMARY = "#0b0b0b"
TEXT_SECONDARY = "#52514e"
GRID = "#dedcd6"

# Categorical slots, assigned in fixed order and never cycled.
BLUE = "#2a78d6"
ORANGE = "#eb6834"
AQUA = "#1baf7a"
YELLOW = "#eda100"

WORKLOAD_LABELS = {
	"cpu_bound": "CPU bound",
	"io_bound": "I/O bound",
	"mixed": "Mixed",
}
WORKLOAD_ORDER = ["cpu_bound", "io_bound", "mixed"]
QUANTA = [1, 2, 4, 8, 16, 32, 64]


def load_results():
	rows = []
	with open(RESULTS) as handle:
		for row in csv.DictReader(handle):
			for key in ("quantum", "boost", "cpus", "processes", "maximum_turnaround", "makespan"):
				row[key] = int(row[key])
			for key in ("average_turnaround", "run_time_ms"):
				row[key] = float(row[key])
			rows.append(row)
	return rows


def find(rows, **conditions):
	for row in rows:
		if all(row[key] == value for key, value in conditions.items()):
			return row
	return None


def style_axes(axes, ylabel=None, xlabel=None, title=None):
	axes.set_facecolor(SURFACE)
	axes.grid(axis="y", color=GRID, linewidth=0.8)
	axes.set_axisbelow(True)
	for side in ("top", "right"):
		axes.spines[side].set_visible(False)
	for side in ("left", "bottom"):
		axes.spines[side].set_color(GRID)
	axes.tick_params(colors=TEXT_SECONDARY, labelsize=9, length=0)
	if title:
		axes.set_title(title, color=TEXT_PRIMARY, fontsize=11, pad=10)
	if ylabel:
		axes.set_ylabel(ylabel, color=TEXT_SECONDARY, fontsize=9)
	if xlabel:
		axes.set_xlabel(xlabel, color=TEXT_SECONDARY, fontsize=9)


def save(figure, name):
	if not os.path.isdir(FIGURE_DIRECTORY):
		os.makedirs(FIGURE_DIRECTORY)
	path = os.path.join(FIGURE_DIRECTORY, name)
	figure.savefig(path, dpi=160, facecolor=SURFACE, bbox_inches="tight")
	plt.close(figure)
	print("wrote " + path)


# ------------------------------------------------- round robin quantum sweep

def plot_quantum_sweep(rows, metric, name, ylabel):
	figure, panels = plt.subplots(1, 3, figsize=(11, 3.6))
	figure.patch.set_facecolor(SURFACE)

	for panel, workload in zip(panels, WORKLOAD_ORDER):
		for cpus, colour, label in ((1, BLUE, "1 processor"), (2, ORANGE, "2 processors")):
			values = [find(rows, workload=workload, algorithm="RR", quantum=q, cpus=cpus)[metric]
			          for q in QUANTA]
			panel.plot(QUANTA, values, color=colour, linewidth=2,
			           marker="o", markersize=5, label=label)

			# FIFO is round robin with an unbounded quantum, so it is the line
			# each curve is walking towards.
			fifo = find(rows, workload=workload, algorithm="FIFO", cpus=cpus)[metric]
			panel.axhline(fifo, color=colour, linewidth=1, linestyle=(0, (4, 3)), alpha=0.7)

		panel.set_xscale("log", base=2)
		panel.set_xticks(QUANTA)
		panel.set_xticklabels([str(q) for q in QUANTA])
		style_axes(panel, xlabel="time quantum", title=WORKLOAD_LABELS[workload])

	panels[0].set_ylabel(ylabel, color=TEXT_SECONDARY, fontsize=9)
	handles, labels = panels[0].get_legend_handles_labels()
	handles.append(plt.Line2D([0], [0], color=TEXT_SECONDARY, linewidth=1,
	                          linestyle=(0, (4, 3))))
	labels.append("the same algorithm under FIFO")
	figure.legend(handles, labels, loc="upper center",
	              bbox_to_anchor=(0.5, 1.10), ncol=3, frameon=False,
	              fontsize=9, labelcolor=TEXT_SECONDARY)
	save(figure, name)


# ------------------------------------------------- algorithm comparison

ALGORITHM_SLOTS = [
	("FIFO", BLUE, dict(algorithm="FIFO")),
	("RR (q = 2)", ORANGE, dict(algorithm="RR", quantum=2)),
	("MLFQ, no boost", AQUA, dict(algorithm="MLFQ", boost=0)),
	("MLFQ, boost 20", YELLOW, dict(algorithm="MLFQ", boost=20)),
]


def plot_algorithm_comparison(rows, metric, name, ylabel):
	figure, panels = plt.subplots(1, 2, figsize=(11, 4.0), sharey=True)
	figure.patch.set_facecolor(SURFACE)

	slot_width = 1.0 / (len(ALGORITHM_SLOTS) + 1)

	for panel, cpus in zip(panels, (1, 2)):
		for slot, (label, colour, conditions) in enumerate(ALGORITHM_SLOTS):
			positions = []
			values = []
			for index, workload in enumerate(WORKLOAD_ORDER):
				row = find(rows, workload=workload, cpus=cpus, **conditions)
				positions.append(index + (slot - 1.5) * slot_width)
				values.append(row[metric])

			# The bar width leaves a small gap so adjacent fills never touch.
			panel.bar(positions, values, width=slot_width * 0.88,
			          color=colour, label=label if cpus == 1 else None)

			# Direct labels, which also cover the two slots that sit below the
			# contrast floor on a light surface.
			for x, value in zip(positions, values):
				panel.annotate("%.0f" % value, (x, value), ha="center", va="bottom",
				               fontsize=7, color=TEXT_SECONDARY, rotation=90,
				               xytext=(0, 3), textcoords="offset points")

		panel.set_xticks(range(len(WORKLOAD_ORDER)))
		panel.set_xticklabels([WORKLOAD_LABELS[w] for w in WORKLOAD_ORDER])
		style_axes(panel, title="%d processor%s" % (cpus, "" if cpus == 1 else "s"))

	panels[0].set_ylabel(ylabel, color=TEXT_SECONDARY, fontsize=9)
	panels[0].set_ylim(0, max(r[metric] for r in rows if r["cpus"] == 1) * 1.22)

	handles, labels = panels[0].get_legend_handles_labels()
	figure.legend(handles, labels, loc="upper center", bbox_to_anchor=(0.5, 1.06),
	              ncol=4, frameon=False, fontsize=9, labelcolor=TEXT_SECONDARY)
	save(figure, name)


# ------------------------------------------------- the MLFQ priority boost

def plot_boost_effect(rows):
	figure, panels = plt.subplots(1, 2, figsize=(9, 3.8), sharey=True)
	figure.patch.set_facecolor(SURFACE)

	for panel, metric in zip(panels, ("average_turnaround", "maximum_turnaround")):
		positions = []
		changes = []
		labels = []
		for index, workload in enumerate(WORKLOAD_ORDER):
			for offset, cpus in ((-0.18, 1), (0.18, 2)):
				without = find(rows, workload=workload, algorithm="MLFQ", boost=0, cpus=cpus)[metric]
				with_boost = find(rows, workload=workload, algorithm="MLFQ", boost=20, cpus=cpus)[metric]
				positions.append(index + offset)
				changes.append(100.0 * (with_boost - without) / without)
				labels.append(cpus)

		colours = [BLUE if c == 1 else ORANGE for c in labels]
		panel.bar(positions, changes, width=0.32, color=colours)
		panel.axhline(0, color=TEXT_SECONDARY, linewidth=1)

		for x, value in zip(positions, changes):
			panel.annotate("%+.2f%%" % value, (x, value), ha="center",
			               va="bottom" if value >= 0 else "top",
			               fontsize=7, color=TEXT_SECONDARY,
			               xytext=(0, 3 if value >= 0 else -3), textcoords="offset points")

		panel.set_xticks(range(len(WORKLOAD_ORDER)))
		panel.set_xticklabels([WORKLOAD_LABELS[w] for w in WORKLOAD_ORDER])
		style_axes(panel, title="average turnaround" if metric == "average_turnaround"
		           else "maximum turnaround")

	panels[0].set_ylabel("change from no boost (%)", color=TEXT_SECONDARY, fontsize=9)
	handles = [plt.Rectangle((0, 0), 1, 1, color=BLUE),
	           plt.Rectangle((0, 0), 1, 1, color=ORANGE)]
	figure.legend(handles, ["1 processor", "2 processors"], loc="upper center",
	              bbox_to_anchor=(0.5, 1.08), ncol=2, frameon=False,
	              fontsize=9, labelcolor=TEXT_SECONDARY)
	save(figure, "mlfq_boost.png")


# ------------------------------------------------- simulator run time

def plot_run_time(rows):
	figure, panel = plt.subplots(figsize=(6.5, 3.8))
	figure.patch.set_facecolor(SURFACE)

	for workload, colour in zip(WORKLOAD_ORDER, (BLUE, ORANGE, AQUA)):
		values = [find(rows, workload=workload, algorithm="RR", quantum=q, cpus=1)["run_time_ms"]
		          for q in QUANTA]
		panel.plot(QUANTA, values, color=colour, linewidth=2, marker="o",
		           markersize=5, label=WORKLOAD_LABELS[workload])
		panel.annotate(WORKLOAD_LABELS[workload], (QUANTA[0], values[0]),
		               xytext=(8, 4), textcoords="offset points",
		               fontsize=8, color=TEXT_SECONDARY, va="bottom")

	panel.set_xscale("log", base=2)
	panel.set_xticks(QUANTA)
	panel.set_xticklabels([str(q) for q in QUANTA])
	style_axes(panel, ylabel="simulator run time (ms, mean of 300)",
	           xlabel="round robin time quantum",
	           title="Cost of simulating a smaller quantum")
	panel.set_xlim(0.85, 80)
	panel.set_ylim(bottom=0)
	panel.legend(frameon=False, fontsize=9, labelcolor=TEXT_SECONDARY, loc="upper right")
	save(figure, "simulator_runtime.png")


# ------------------------------------------------- two processors

def plot_speedup(rows):
	figure, panel = plt.subplots(figsize=(7.5, 3.8))
	figure.patch.set_facecolor(SURFACE)

	slot_width = 1.0 / (len(ALGORITHM_SLOTS) + 1)

	for slot, (label, colour, conditions) in enumerate(ALGORITHM_SLOTS):
		positions = []
		values = []
		for index, workload in enumerate(WORKLOAD_ORDER):
			one = find(rows, workload=workload, cpus=1, **conditions)["average_turnaround"]
			two = find(rows, workload=workload, cpus=2, **conditions)["average_turnaround"]
			positions.append(index + (slot - 1.5) * slot_width)
			values.append(one / two)

		panel.bar(positions, values, width=slot_width * 0.88, color=colour, label=label)
		for x, value in zip(positions, values):
			panel.annotate("%.2f" % value, (x, value), ha="center", va="bottom",
			               fontsize=7, color=TEXT_SECONDARY,
			               xytext=(0, 3), textcoords="offset points")

	panel.axhline(1.0, color=TEXT_SECONDARY, linewidth=1, linestyle=(0, (4, 3)))
	panel.set_xticks(range(len(WORKLOAD_ORDER)))
	panel.set_xticklabels([WORKLOAD_LABELS[w] for w in WORKLOAD_ORDER])
	style_axes(panel, ylabel="speedup in average turnaround",
	           title="What the second processor buys")
	panel.set_ylim(0, 2.75)
	panel.legend(frameon=False, fontsize=9, labelcolor=TEXT_SECONDARY, ncol=4,
	             loc="upper center", bbox_to_anchor=(0.5, 1.02))
	save(figure, "speedup.png")


# ------------------------------------------------- short against long processes

PER_PROCESS_DIRECTORY = "per_process"
PER_PROCESS_SLOTS = [
	("FIFO", BLUE, "fifo"),
	("RR (q = 2)", ORANGE, "rr_q2"),
	("MLFQ, no boost", AQUA, "mlfq"),
	("MLFQ, boost 20", YELLOW, "mlfq_boost20"),
]


def plot_by_process_class():
	# The overall average hides what these policies actually do differently, so
	# split the processes at the median total CPU demand and look at the two
	# halves separately.
	tables = {}
	for _, _, tag in PER_PROCESS_SLOTS:
		path = os.path.join(PER_PROCESS_DIRECTORY, "mixed_" + tag + ".csv")
		with open(path) as handle:
			tables[tag] = {int(row["pid"]): (int(row["total_cpu_demand"]),
			                                 int(row["turnaround_time"]))
			               for row in csv.DictReader(handle)}

	reference = tables[PER_PROCESS_SLOTS[0][2]]
	demands = sorted(demand for demand, _ in reference.values())
	median = demands[len(demands) // 2]

	figure, panel = plt.subplots(figsize=(7.5, 4.0))
	figure.patch.set_facecolor(SURFACE)

	slot_width = 1.0 / (len(PER_PROCESS_SLOTS) + 1)

	for slot, (label, colour, tag) in enumerate(PER_PROCESS_SLOTS):
		positions = []
		values = []
		for index, wants_short in enumerate((True, False)):
			selected = [turnaround for demand, turnaround in tables[tag].values()
			            if (demand <= median) == wants_short]
			positions.append(index + (slot - 1.5) * slot_width)
			values.append(sum(selected) / float(len(selected)))

		panel.bar(positions, values, width=slot_width * 0.88, color=colour, label=label)
		for x, value in zip(positions, values):
			panel.annotate("%.0f" % value, (x, value), ha="center", va="bottom",
			               fontsize=8, color=TEXT_SECONDARY,
			               xytext=(0, 3), textcoords="offset points")

	panel.set_xticks([0, 1])
	panel.set_xticklabels(["short processes\n(CPU demand <= %d)" % median,
	                       "long processes\n(CPU demand > %d)" % median])
	style_axes(panel, ylabel="average turnaround time",
	           title="Mixed workload, one processor: who pays for the policy")
	panel.set_ylim(0, 16500)
	panel.legend(frameon=False, fontsize=9, labelcolor=TEXT_SECONDARY, ncol=2,
	             loc="upper right")
	save(figure, "mixed_by_process_class.png")


def main():
	rows = load_results()
	plot_quantum_sweep(rows, "average_turnaround", "rr_quantum_average.png",
	                   "average turnaround time")
	plot_quantum_sweep(rows, "maximum_turnaround", "rr_quantum_maximum.png",
	                   "maximum turnaround time")
	plot_algorithm_comparison(rows, "average_turnaround", "algorithm_average.png",
	                          "average turnaround time")
	plot_algorithm_comparison(rows, "maximum_turnaround", "algorithm_maximum.png",
	                          "maximum turnaround time")
	plot_boost_effect(rows)
	plot_run_time(rows)
	plot_speedup(rows)
	plot_by_process_class()


if __name__ == "__main__":
	main()
