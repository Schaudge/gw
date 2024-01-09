"""
Arguments
---------
glob_pattern
tag

Usage
-----

python3 plot.py '*1.benchmark.csv' threads_1
"""
import pandas as pd
import seaborn as sns
import glob
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import sys
pd.options.mode.chained_assignment = None

glob_pattern = sys.argv[1]
tag = sys.argv[2]
tables = glob.glob(glob_pattern)
print(tables)
dfs = []
for p in tables:
    d = pd.read_csv(p)
    d['file'] = p
    dfs.append(d)
df = pd.concat(dfs)
df['RSS'] = df['RSS'] / 1e6
print(df.columns)
gw_times = {}
gw_mem = {}
samtools = {}
for idx, grp in df[df['name'] == 'gw'].groupby('region size (bp)'):
    gw_times[idx] = grp['time (s)'].mean()
    gw_mem[idx] = grp['RSS'].mean()
for idx, grp in df.groupby('region size (bp)'):
    samtools[idx] = grp['samtools_count (s)'].mean()

print(gw_times)
# use the mean time of 2bp region as start_time
min_load_time = {k: dd['time (s)'].min() for k, dd in df[df['region size (bp)'] == 2].groupby('name')}
min_memory = {k: dd['RSS'].min() for k, dd in df[df['region size (bp)'] == 2].groupby('name')}
df['total_time'] = df['time (s)']
df['start_time'] = [min_load_time[k] for k in df['name']]
df['total_mem'] = df['RSS']
df['start_mem'] = [min_memory[k] for k in df['name']]
df = df[df['region size (bp)'] != 2]

su = []
for idx, grp in df.groupby(['name', 'region size (bp)']):
    su.append({'name': idx[0],
               'region size (bp)': idx[1],
               'samtools': samtools[idx[1]],
               'total_time': grp['total_time'].mean(),
               'start_time': grp['start_time'].mean(),
               'render': grp['total_time'].mean() - grp['start_time'].mean(),
               'total_mem': grp['total_mem'].mean(),
               'start_mem': grp['start_mem'].mean(),
               'relative_time': grp['total_time'].mean() / gw_times[idx[1]],
               "relative_mem": grp['total_mem'].mean() / gw_mem[idx[1]]})

df2 = pd.DataFrame.from_records(su).round(3)
gw_render_times = {k: t for k, t in zip(df2[df2['name'] == 'gw']['region size (bp)'],
                                        df2[df2['name'] == 'gw']['render'])}
df2['relative_render_time'] = [k / gw_render_times[s] for k, s in zip(df2['render'], df2['region size (bp)'])]
df2 = df2[['name', 'region size (bp)', 'samtools', 'total_time', 'relative_time',
           'start_time', 'render', 'relative_render_time', 'total_mem', 'start_mem', 'relative_mem']]
print(df2.to_markdown())
with open(f'benchmark.{tag}.md', 'w') as b:
    b.write(df2.to_markdown())

order = {'gw': 0, 'igv': 1, 'jb2export': 2, 'samplot': 3, 'wally': 4, 'bamsnap': 5, 'genomeview': 6}
df2['srt'] = [order[k] for k in df2['name']]
df2.sort_values('srt', inplace=True)
custom_params = {"axes.spines.right": False, "axes.spines.top": False}
sns.set_theme(style="ticks", rc=custom_params)

for item in ['total_time', 'relative_time', 'render', 'relative_render_time', 'start_time', 'total_mem', 'start_mem', 'relative_mem']:
    g = sns.catplot(data=df2,
                     palette="tab10", x='region size (bp)', y=item, hue='name',
                     kind='point')

    g.ax.set_xlabel("Region size (bp)", fontsize=14)
    label = list(item.replace('_', ' '))
    label[0] = label[0].upper()
    g.ax.set_ylabel(''.join(label), fontsize=14)
    g.ax.tick_params(labelsize=15)
    g.ax.set_yscale('log')
    g.set_xticklabels(rotation=30)
    plt.subplots_adjust(left=0.15)
    plt.subplots_adjust(bottom=0.25)
    labels = [item.get_text() for item in g.ax.get_xticklabels()]
    labels = ["{:,}".format(int(l)) for l in labels]
    g.ax.set_xticklabels(labels)
    plt.savefig(f'plots/benchmark_{item}_log.pdf')
    plt.close()
# plt.show()

quit()



fig, axes = plt.subplots(nrows=4, ncols=1, figsize=(7, 6))
fig.subplots_adjust(bottom=0.15, hspace=0.8)

idx = 0
for size, grp in df2.groupby('region size (bp)'):
    ax = sns.barplot(y='name', x='relative_time', data=grp,
                     color='royalblue', ax=axes[idx], errorbar=('ci', 95), linewidth=0, err_kws={'linewidth': 1})
    # ax2 = sns.barplot(y='name', x='relative_time', data=grp,
    #                   color='lightsteelblue', ax=axes[idx], errorbar=('ci', 95), linewidth=0, err_kws={'linewidth': 1})

    sns.despine(left=True, bottom=True)
    ax.grid(axis='x')
    if size == 2000:
        ax.set_title('2 kb')
    elif size == 20_000:
        ax.set_title('20 kb')
    elif size == 200_000:
        ax.set_title('200 kb')
    elif size == 2_000_000:
        ax.set_title('2 Mb')
    if idx == 3:
        ax.set_xlabel('Time (s)')
    else:
        ax.set_xlabel('')
    ax.set_ylabel('')
    ax.set_xscale('log')
    idx += 1

b_patch = mpatches.Patch(color='royalblue', label='Total')
b2_patch = mpatches.Patch(color='lightsteelblue', label='Start')
plt.legend(loc='lower center', ncol=2, handles=[b_patch, b2_patch], bbox_to_anchor=(0.5, -1.2),
           prop={'size': 10})
plt.savefig(f'time.{tag}.png')
plt.show()
plt.close()

exit()

fig, axes = plt.subplots(nrows=4, ncols=1, figsize=(7, 6))
fig.subplots_adjust(bottom=0.15, hspace=0.8)
idx = 0
for size, grp in df.groupby('region size (bp)'):
    ax = sns.barplot(y='name', x='total_mem', data=grp,
                     color='firebrick', ax=axes[idx], errorbar=('ci', 95), linewidth=0, err_kws={'linewidth': 1})
    ax2 = sns.barplot(y='name', x='start_mem', data=grp,
                     color='rosybrown', ax=axes[idx], errorbar=('ci', 95), linewidth=0, err_kws={'linewidth': 1})
    sns.despine(left=True, bottom=True)
    ax.grid(axis='x')
    if size == 2000:
        ax.set_title('2 kb')
    elif size == 20_000:
        ax.set_title('20 kb')
    elif size == 200_000:
        ax.set_title('200 kb')
    elif size == 2_000_000:
        ax.set_title('2 Mb')
    if idx == 3:
        ax.set_xlabel('Resident set size (GB)')
    else:
        ax.set_xlabel('')
    ax.set_ylabel('')
    idx += 1
b_patch = mpatches.Patch(color='firebrick', label='Total')
b2_patch = mpatches.Patch(color='rosybrown', label='Start')
plt.legend(loc='lower center', ncol=2, handles=[b_patch, b2_patch], bbox_to_anchor=(0.5, -1.2),
           prop={'size': 10})
plt.savefig(f'memory.{tag}.png')
plt.show()
plt.close()
