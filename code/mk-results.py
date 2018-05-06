#!/usr/bin/env python

from __future__ import print_function
import json
import subprocess
import sys

iters = 10

devnull = "/dev/null"

in_fnames = [
    "./img/bluejay.ppm",
    "./img/elephant.ppm",
    "./img/flower.ppm",
    "./img/large_elephant.ppm",
    "./img/purp.ppm",
    "./img/tiger.ppm",
]

cpp_prog = "./main-cpp"
omp_prog = "./main-omp"
cuda_prog = "./main-cu"
ispc_prog = "./main-ispc"

programs = [
    cpp_prog,
    omp_prog,
    cuda_prog,
    ispc_prog,
]

time_items = [
    u"init",
    u"color_counts",
    u"build_mask",
    u"refine_mask",
    u"blur",
    u"clean_up",
]

def dict_sum(a):
    ret = 0
    for x in a:
        ret += a[x]
    return ret

def dict_is_lt(a, b):
    return dict_sum(a) - dict_sum(b) < 0

data = {}

for prog in programs:
    data[prog] = {}
    for in_f in in_fnames:
        data[prog][in_f] = None
        for i in range(iters):
            print(prog, in_f, i, file=sys.stderr)
            out = subprocess.check_output([prog, in_f, devnull])
            new = json.loads(out)
            cur = data[prog][in_f]
            if cur is None or dict_is_lt(new, cur):
                data[prog][in_f] = new

table_begin = "\\begin{tabular}{r|r|r|r|r|r|r|r}"
with_slash = "\\\\  "
no_slash = "    "
row_header = no_slash + (
    "Item & C++ & "
    "OMP & Speedup & "
    "CUDA & Speedup & "
    "ISPC & Speedup"
)
row = (
    "{} & {:.4f} & "
    "{:.4f} & {:.4f} & "
    "{:.4f} & {:.4f} & "
    "{:.4f} & {:.4f}"
)

for in_f in in_fnames:
    print("\\subsection{" + in_f + "}")
    print(table_begin)
    print(row_header)
    print(with_slash + "\\hline")
    cpp_sum = 0
    omp_sum = 0
    cuda_sum = 0
    ispc_sum = 0
    first = True
    for ti in time_items:
        disp = str(ti).replace("_", " ")
        cpp = data[cpp_prog][in_f][ti]
        omp = data[omp_prog][in_f][ti]
        cuda = data[cuda_prog][in_f][ti]
        ispc = data[ispc_prog][in_f][ti]
        cpp_sum += cpp
        omp_sum += omp
        cuda_sum += cuda
        ispc_sum += ispc
        if first:
            print(no_slash, end="")
        else:
            print(with_slash, end="")
        first = False
        print(row.format(disp,
            cpp,
            omp,
            cpp / omp,
            cuda,
            cpp / cuda,
            ispc,
            cpp / ispc
        ))
    print(with_slash + "\\hline")
    print(no_slash + row.format(
        "total",
        cpp_sum,
        omp_sum,
        cpp_sum / omp_sum,
        cuda_sum,
        cpp_sum / cuda_sum,
        ispc_sum,
        cpp_sum / ispc_sum
    ))
    print("\\end{tabular}")