| name       |   region size (bp) |   samtools |   total_time |   relative_time |   start_time |   render |   relative_render_time |   total_mem |   start_mem |   relative_mem |
|:-----------|-------------------:|-----------:|-------------:|----------------:|-------------:|---------:|-----------------------:|------------:|------------:|---------------:|
| gw         |               2000 |      0.021 |        0.085 |           1     |        0.082 |    0.003 |                  1     |       0.048 |       0.047 |          1     |
| gw         |              20000 |      0.022 |        0.103 |           1     |        0.082 |    0.021 |                  1     |       0.048 |       0.047 |          1     |
| gw         |             200000 |      0.039 |        0.136 |           1     |        0.082 |    0.054 |                  1     |       0.052 |       0.047 |          1     |
| gw         |            2000000 |      0.197 |        0.279 |           1     |        0.082 |    0.196 |                  1     |       0.063 |       0.047 |          1     |
| gw -t4     |               2000 |      0.022 |        0.086 |           1.018 |        0.082 |    0.004 |                  1.56  |       0.049 |       0.047 |          1.027 |
| gw -t4     |              20000 |      0.022 |        0.104 |           1.008 |        0.082 |    0.022 |                  1.039 |       0.05  |       0.047 |          1.039 |
| gw -t4     |             200000 |      0.026 |        0.135 |           0.99  |        0.082 |    0.053 |                  0.975 |       0.054 |       0.047 |          1.036 |
| gw -t4     |            2000000 |      0.064 |        0.251 |           0.9   |        0.082 |    0.169 |                  0.858 |       0.065 |       0.047 |          1.032 |
| igv        |               2000 |      0.021 |        3.402 |          40.051 |        3.258 |    0.143 |                 51.843 |       0.261 |       0.214 |          5.459 |
| igv        |              20000 |      0.022 |        4.084 |          39.577 |        3.258 |    0.826 |                 39.276 |       0.397 |       0.214 |          8.226 |
| igv        |             200000 |      0.039 |        8.621 |          63.206 |        3.258 |    5.363 |                 98.892 |       1.011 |       0.214 |         19.444 |
| igv        |            2000000 |      0.197 |       39.79  |         142.861 |        3.258 |   36.532 |                186.048 |       6.82  |       0.214 |        109.013 |
| igv -t4    |               2000 |      0.022 |        3.426 |          40.333 |        3.258 |    0.167 |                 60.485 |       0.24  |       0.214 |          5.024 |
| igv -t4    |              20000 |      0.022 |        4.116 |          39.89  |        3.258 |    0.858 |                 40.808 |       0.365 |       0.214 |          7.55  |
| igv -t4    |             200000 |      0.026 |        8.59  |          62.982 |        3.258 |    5.332 |                 98.329 |       1.054 |       0.214 |         20.281 |
| igv -t4    |            2000000 |      0.064 |       33.439 |         120.058 |        3.258 |   30.181 |                153.702 |       6.559 |       0.214 |        104.851 |
| jb2export  |               2000 |      0.021 |        2.683 |          31.584 |        2.565 |    0.118 |                 42.625 |       0.312 |       0.313 |          6.526 |
| jb2export  |              20000 |      0.022 |        3.086 |          29.904 |        2.565 |    0.521 |                 24.791 |       0.349 |       0.313 |          7.233 |
| jb2export  |             200000 |      0.039 |        6.543 |          47.973 |        2.565 |    3.979 |                 73.369 |       0.835 |       0.313 |         16.058 |
| jb2export  |            2000000 |      0.197 |       34.292 |         123.121 |        2.565 |   31.727 |                161.58  |       4.237 |       0.313 |         67.728 |
| samplot    |               2000 |      0.021 |        0.552 |           6.496 |        0.497 |    0.055 |                 19.713 |       0.106 |       0.104 |          2.212 |
| samplot    |              20000 |      0.022 |        0.689 |           6.678 |        0.497 |    0.192 |                  9.132 |       0.111 |       0.104 |          2.295 |
| samplot    |             200000 |      0.039 |        1.858 |          13.622 |        0.497 |    1.361 |                 25.093 |       0.167 |       0.104 |          3.21  |
| samplot    |            2000000 |      0.197 |       13.205 |          47.411 |        0.497 |   12.708 |                 64.719 |       0.803 |       0.104 |         12.834 |
| genomeview |               2000 |      0.021 |        0.288 |           3.387 |        0.174 |    0.114 |                 41.129 |       0.103 |       0.103 |          2.163 |
| genomeview |              20000 |      0.022 |        1.398 |          13.544 |        0.174 |    1.224 |                 58.206 |       0.105 |       0.103 |          2.178 |
| genomeview |             200000 |      0.039 |       12.142 |          89.02  |        0.174 |   11.968 |                220.696 |       0.13  |       0.103 |          2.499 |
| genomeview |            2000000 |      0.197 |      126.82  |         455.329 |        0.174 |  126.646 |                644.976 |       0.369 |       0.103 |          5.898 |