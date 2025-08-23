# 简化版线程性能测试绘图脚本
# 依赖：matplotlib, numpy
import matplotlib.pyplot as plt
import numpy as np
import argparse

# 读取结果文件，返回线程数和执行时间列表
def read_results_file(file_path):
    threads, times = [], []
    with open(file_path, 'r') as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            if ',' in line:
                t, tm = line.split(',')
                threads.append(int(t))
                times.append(float(tm))
    return threads, times

# 读取包含多组结果的文件。组与组之间以一行空行分隔。
def read_results_multiple(file_path):
    groups = []
    cur_threads, cur_times = [], []
    with open(file_path, 'r') as f:
        for raw in f:
            line = raw.strip()
            # 空行作为组分隔符
            if line == '':
                if cur_threads:
                    groups.append((cur_threads, cur_times))
                    cur_threads, cur_times = [], []
                continue
            if line.startswith('#'):
                continue
            if ',' in line:
                t, tm = line.split(',')
                cur_threads.append(int(t))
                cur_times.append(float(tm))
    if cur_threads:
        groups.append((cur_threads, cur_times))
    return groups

# 绘制性能图表，支持多条曲线
def plot(thread_nums, exec_times, baseline=63.32, title=None, output=None, labels=None):
    plt.figure(figsize=(12,8))

    # 如果传入的是多组数据，绘制多条曲线
    if isinstance(exec_times[0], (list, tuple)):
        series = exec_times
        threads_ref = thread_nums
        colors = ['C0', 'C1', 'C2', 'C3', 'C4']
        for idx, ser in enumerate(series):
            lbl = (labels[idx] if labels and idx < len(labels) else f'Series {idx+1}')
            plt.plot(threads_ref, ser, marker='o', linestyle='-', label=lbl, color=colors[idx % len(colors)], markersize=6, linewidth=2)
    else:
        plt.plot(thread_nums, exec_times, 'bo-', label='Execution Time', markersize=6, linewidth=2)

    plt.axhline(y=baseline, color='r', linestyle='--', linewidth=2, label=f'Baseline {baseline}s')

    # 设置坐标轴
    plt.xlabel('Thread Number', fontsize=12)
    plt.ylabel('Execution Time (s)', fontsize=12)
    plt.title(title or 'Thread Performance Analysis', fontsize=14, pad=20)

    # 横坐标：每个线程数对应一个刻度
    plt.xticks(thread_nums, fontsize=8)

    # 纵坐标：更细分的刻度
    y_min, y_max = min([min(s) if isinstance(s, (list, tuple)) else s for s in (exec_times if isinstance(exec_times[0], (list, tuple)) else [exec_times])]) , max([max(s) if isinstance(s, (list, tuple)) else s for s in (exec_times if isinstance(exec_times[0], (list, tuple)) else [exec_times])])
    y_range = y_max - y_min
    if y_range > 0:
        # 计算合适的刻度间隔
        if y_range <= 1:
            tick_interval = 0.1
        elif y_range <= 5:
            tick_interval = 0.5
        elif y_range <= 10:
            tick_interval = 1
        elif y_range <= 50:
            tick_interval = 5
        else:
            tick_interval = 10

        # 设置纵坐标范围和刻度
        y_min_tick = (int(y_min / tick_interval) - 1) * tick_interval
        y_max_tick = (int(y_max / tick_interval) + 2) * tick_interval
        yticks = np.arange(y_min_tick, y_max_tick, tick_interval)
        plt.yticks(yticks)

        # 设置纵坐标显示范围
        plt.ylim(y_min_tick, y_max_tick)

    # 添加网格和图例
    plt.grid(True, alpha=0.3, linestyle='-', linewidth=0.5)
    plt.legend(fontsize=10)

    # 调整布局
    plt.tight_layout()

    if output:
        plt.savefig(output, dpi=300, bbox_inches='tight')
    else:
        plt.show()
    plt.close()

def main():
    parser = argparse.ArgumentParser(description='线程性能测试图表')
    parser.add_argument('--input', '-i', type=str, help='输入结果文件路径')
    parser.add_argument('--output', '-o', type=str, help='输出图片文件路径')
    parser.add_argument('--baseline', '-b', type=float, default=63.32, help='基准线时间')
    parser.add_argument('--title', type=str, default='', help='图表标题')
    parser.add_argument('--sample', '-s', action='store_true', help='使用示例数据')
    args = parser.parse_args()

    # 示例数据或文件数据
    if args.sample:
        thread_nums = list(range(1, 65))
        exec_times = [100.0 if t==1 else 100.0*(0.3**((t-1)/7)) if t<=8 else 22.0+np.random.normal(0,1.5) for t in thread_nums]
        exec_times = [max(18, min(26, tm)) if t>8 else tm for t,tm in zip(thread_nums,exec_times)]
        # 生成第二组示例以演示双曲线绘制
        exec_times2 = [et * (0.95 + 0.05 * np.random.randn()) for et in exec_times]
        plot(thread_nums, [exec_times, exec_times2], baseline=args.baseline, title=args.title, output=args.output, labels=['Run A','Run B'])
    elif args.input:
        groups = read_results_multiple(args.input)
        if len(groups) == 0:
            print('No data found in input file.'); return
        # 如果只有一组，保持兼容行为
        if len(groups) == 1:
            thread_nums, exec_times = groups[0]
            plot(thread_nums, exec_times, baseline=args.baseline, title=args.title, output=args.output)
        else:
            # 假设每组使用相同的 thread_nums（通常成立），使用第一组的线程刻度
            thread_nums = groups[0][0]
            series = [g[1] for g in groups]
            labels = [f'Run {i+1}' for i in range(len(series))]
            plot(thread_nums, series, baseline=args.baseline, title=args.title, output=args.output, labels=labels)
    else:
        parser.print_help(); return

    # 如果没有提前退出
    if args.sample:
        return

if __name__ == '__main__':
    main()
