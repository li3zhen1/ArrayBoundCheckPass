#import "homework-template.typ": *

#show: doc => conf(title: "CS6241: Project 1 - Part 1", doc)

#let results = csv("stat_compile_time/check_elimination.txt")

#outline(indent: true)

#let benchs = (
  "is",
  "bfs",
  "dither",
  "jacobi-1d",
  // "malloc_1d_array",
  // "static_1d_array",
  // "global_1d_array",
  "check_elimination",
  "check_modification",
)

#let check_stages = ("original", "transformed",)

#let formatNumber(number, precision) = {
  assert(precision > 0)
  let s = str(calc.round(number, digits: precision))
  let tail = s.find(regex("\\..*"))
  let pad = if tail == none {
    s = s + "."
    precision
  } else {
    precision - tail.len() + 1
  }
  s + pad * "0"
}

#let rounded(x) = formatNumber(x * 1000, 3)
#let percentage(x) = formatNumber(x * 100, 2) + "%"
#let hl = table.hline(stroke: .6pt)
#let hl2 = table.hline(stroke: .3pt)

= Baseline
#let stat_baseline_size = csv("stat_baseline_size.csv").flatten()

#let render_perf_table(bench) = {
  let idx = stat_baseline_size.position(it => it == bench)
  let fsize1 = float(stat_baseline_size.at(idx + 1))
  let fsize2 = float(stat_baseline_size.at(idx + 2))

  let stage = "original"
  let fo = csv("stat_baseline/" + bench + "-" + stage + ".csv").flatten()
  let _n(arr, i) = rounded(float(arr.at(i)))
  let r1 = (
    table.cell(rowspan: 3)[#bench],
    stage,
    [#fsize1],
    _n(fo, 12),
    _n(fo, 13),
    _n(fo, 9),
  )
  let stage = "transformed"
  let ft = csv("stat_baseline/" + bench + "-" + stage + ".csv").flatten()
  let r2 = (stage, [#fsize2], _n(ft, 12), _n(ft, 13), _n(ft, 9),)

  let r3 = (
    [*ratio*],
    [*#percentage(fsize2 / fsize1)*],
    [*#percentage(float(ft.at(12)) / float(fo.at(12)))*],
    [*#percentage(float(ft.at(13)) / float(fo.at(13)))*],
    [*#percentage(float(ft.at(9)) / float(fo.at(9)))*],
  )

  (hl, ..r1, ..r2, hl2, ..r3)
}

#text(features: ("tnum": 1))[
  #set table(align: (x, _) => if x < 2 { left } else { right })
  #table(
    columns: (1.2fr, 0.8fr, 1fr, 1fr, 1fr, 1fr),
    stroke: none,
    hl,
    table.header(
      [*Bench*],
      [],
      align(right)[*Bytecode size (byte)*],
      align(right)[*User \ (ms)*],
      align(right)[*System \ (ms)*],
      align(right)[*Mean Total \ (ms)*],
    ),
    ..render_perf_table("is"),
    ..render_perf_table("bfs"),
    ..render_perf_table("dither"),
    ..render_perf_table("jacobi-1d"),
    ..render_perf_table("check_elimination"),
    ..render_perf_table("check_modification"),
    hl,
  )]

= Gupta's methods

== Compile time check stats

=== check_elimination

#table(
  columns: (1.5fr, 3fr, 1fr, 1fr, 1fr),
  [*Function*],
  [*stage*],
  [*Lower*],
  [*Upper*],
  [*Total*],
  ..results.flatten(),
)

== Runtime check stats

== Static spill code generated in each commenting on causes of performance degradations

