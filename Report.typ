#import "homework-template.typ": *

#show: doc => conf(title: "CS6241: Project 1 - Part 1", doc)

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
#let percentage(x) = formatNumber(x * 100, 1) + "%"
#let hl = table.hline(stroke: .8pt)
#let hl2 = table.hline(stroke: .3pt)
#let header_fill = (_, y) => if y == 0 { rgb("#f2f3f6") }

#v(2.5em)

I did this alone.

\

= Baseline
#let stat_baseline_size = csv("stat/_stat_baseline_size.csv").flatten()

#let render_perf_table(folder, bench, size_csv, tranform_name: "baseline") = {
  let idx = size_csv.position(it => it == bench)
  let fsize1 = float(size_csv.at(idx + 1))
  let fsize2 = float(size_csv.at(idx + 2))

  let stage = "original"
  let fo = csv(folder + "/" + bench + "-" + stage + ".csv").flatten()
  let _n(arr, i) = rounded(float(arr.at(i)))
  let r1 = (
    table.cell(rowspan: 3)[#bench],
    stage,
    table.vline(stroke: .3pt),
    [#fsize1],
    _n(fo, 12),
    _n(fo, 13),
    _n(fo, 9),
  )
  let stage = "transformed"
  let ft = csv(folder + "/" + bench + "-" + stage + ".csv").flatten()
  let r2 = (tranform_name, [#fsize2], _n(ft, 12), _n(ft, 13), _n(ft, 9),)

  let r3 = (
    [*ratio*],
    [*#percentage(fsize2 / fsize1)*],
    [*#percentage(float(ft.at(12)) / float(fo.at(12)))*],
    [*#percentage(float(ft.at(13)) / float(fo.at(13)))*],
    [*#percentage(float(ft.at(9)) / float(fo.at(9)))*],
  )

  (hl, ..r1, ..r2, hl2, ..r3)
}

#v(0.5em)

The statistics are generated with `./gen_baseline_stat.py`.

The llvm bytecode size are measured by python script `os.path.get_size()`.

The time are measured by a `time` based profiling tool `hyperfine`. It runs the
program 2 times for warmup and then 10 times to take the average. (macOS seems
do not have equivalent to `perf` on Linux.)

#text(
  features: ("tnum": 1),
)[
#set table(align: (x, _) => if x < 2 { left } else { right })
#set text(size: 8.5pt)
#figure(
  table(
    columns: (1.3fr, 0.7fr, 1fr, 1fr, 1fr, 1fr),
    stroke: none,
    fill: header_fill,
    hl,
    table.header(
      [*Bench*],
      [],
      [*Bytecode Size (byte)*],
      [*Mean User\ (ms)*],
      [*Mean System\ (ms)*],
      [*Mean Total\ (ms)*],
    ),
    ..{
      for bench in benchs {
        render_perf_table("stat/_stat_baseline", bench, stat_baseline_size)
      }
    },
    hl,
  ),
  caption: [Performance comparison between original programs and after the `check-ins` pass],
)
]


#pagebreak()
= Gupta's methods


== Bytecode size and performance

#v(0.5em)


The statistics are generated with `./gen_gupta_stat.py`. The measurements are
the same as the baseline.

#let render_perf_table(folder, bench, size_csv, tranform_name: "baseline") = {
  let idx = size_csv.position(it => it == bench)
  let fsize1 = float(size_csv.at(idx + 1))
  let fsize2 = float(size_csv.at(idx + 2))

  let fsize2_baseline = float(stat_baseline_size.at(idx + 2))

  let stage = "original"
  let fo = csv("stat/_stat_baseline" + "/" + bench + "-" + stage + ".csv").flatten()
  let _n(arr, i) = rounded(float(arr.at(i)))
  let r1 = (
    table.cell(rowspan: 4)[#bench],
    stage,
    table.vline(stroke: .3pt),
    [#fsize1],
    [100.0%],
    _n(fo, 12),
    [100.0%],
    _n(fo, 9),
    [100.0%],
  )
  let stage = "transformed"
  let ft = csv("stat/_stat_baseline" + "/" + bench + "-" + stage + ".csv").flatten()
  let r2 = (
    [baseline],
    [#fsize2_baseline],
    percentage(fsize2_baseline / fsize1),
    _n(ft, 12),
    percentage(float(ft.at(12)) / float(fo.at(12))),
    // _n(ft, 13),
    _n(ft, 9),
    percentage(float(ft.at(9)) / float(fo.at(9))),
  )

  let stage = "transformed"
  let ft_gupta = csv(folder + "/" + bench + "-" + stage + ".csv").flatten()
  let r3 = (
    tranform_name,
    [#fsize2],
    percentage(fsize2 / fsize1),
    _n(ft_gupta, 12),
    percentage(float(ft_gupta.at(12)) / float(fo.at(12))),
    _n(ft_gupta, 9),
    percentage(float(ft_gupta.at(9)) / float(fo.at(9))),
  )

  let r4 = (
    text(
      font: "SF-TrueType Pro Display",
      weight: 600,
    )[(#tranform_name - baseline)/original],
    [],
    [*#percentage((fsize2 - fsize2_baseline) / fsize1)*],
    [],
    [*#percentage((float(ft_gupta.at(12)) - float(ft.at(12))) / float(fo.at(12)))*],
    [],
    [*#percentage((float(ft_gupta.at(9)) - float(ft.at(9))) / float(fo.at(9)))*],
  )
  (hl, ..r1, ..r2, ..r3, hl2, ..r4)
}

#let stat_gupta_size = csv("stat_gupta_size.csv").flatten()
#text(
  features: ("tnum": 1),
)[
#set table(align: (x, y) => if (x < 2) or (calc.odd(x)) { left } else { right })
#set text(size: 8.5pt)
#figure(
  table(
    columns: (1.7fr, 2.2fr, 0.8fr, 1fr, 1fr, 1fr, 1fr, 1fr),
    stroke: none,
    fill: header_fill,
    hl,
    table.header(
      [*Bench*],
      [],
      table.cell(colspan: 2, align(center)[*Bytecode Size (byte)*]),
      table.cell(colspan: 2, align(center)[*Mean User Time (ms)*]),
      table.cell(colspan: 2, align(center)[*Mean Total Time (ms)*]),
    ),
    ..{
      for bench in benchs {
        render_perf_table("stat_gupta", bench, stat_gupta_size, tranform_name: "gupta")
      }
    },
    hl,
  ),
  caption: [Performance comparison between original programs and after the `check-opt` pass],
)
]

#let render_check_count_table(folder, bench) = {
  let count_csv = csv(folder + "/" + bench + ".csv").flatten()
  (
    hl2,
    table.cell(rowspan: 1, bench),
    table.vline(stroke: .3pt),
    count_csv.at(3),

    count_csv.at(7),

    count_csv.at(11),

    count_csv.at(15),
    // hl2,
    // [*Percentage removed by `check-opt`*],
    // [],
    // [],
    [*#percentage(1 - float(count_csv.at(15)) / float(count_csv.at(3)))*],
  )
}

\ 

== Compile time check counts

#let benchs = (
  "is",
  "bfs",
  "dither",
  "jacobi-1d",
  "check_elimination",
  "check_modification",
  "malloc_1d_array",
  "static_1d_array",
  "global_1d_array",
)


#text(
  features: ("tnum": 1),
)[
#set table(align: (x, y) => if (x < 1) { left } else { right })
#set text(size: 8.5pt)
// #set par(leading:0.3em)
#figure(
  table(
    columns: (1.5fr, 1fr, 1fr, 1fr, 1fr, 1fr),
    stroke: none,
    // inset: (top:0.3em, bottom:0.4em),
    fill: header_fill,
    hl,
    table.header(
      [*Bench*],
      [*After \ Insertion*],
      [*After Modification*],
      [*After Elimination*],
      [*After Loop Hoisting*],
      [*Percentage removed*],
    ),
    ..{
      for bench in benchs {
        render_check_count_table("stat/_stat_check_count", bench)
      }
    },
    hl,
  ),
  caption: [Bound check counts comparison between `check-ins` and `check-opt` pass (compile time)], 
)
]




// == Runtime check counts

== Static spill code generated in each commenting on causes of performance degradations


== Detailed analysis 

About why certain benchmarks show a lot of removal opportunities whereas others dont, why removal is co-related to performance improvement in some cases whereas not co-related or less co-related in others and finally comparison of effectiveness

