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

# 绘制性能图表
def plot(thread_nums, exec_times, baseline=63.32, title=None, output=None):
    plt.figure(figsize=(12,8))
    
    # 绘制数据点和线
    plt.plot(thread_nums, exec_times, 'bo-', label='Execution Time', markersize=6, linewidth=2)
    plt.axhline(y=baseline, color='r', linestyle='--', linewidth=2, label=f'Baseline {baseline}s')
    
    # 设置坐标轴
    plt.xlabel('Thread Number', fontsize=12)
    plt.ylabel('Execution Time (s)', fontsize=12)
    plt.title(title or 'Thread Performance Analysis', fontsize=14, pad=20)
    
    # 横坐标：每个线程数对应一个刻度
    plt.xticks(thread_nums, fontsize=8)
    
    # 纵坐标：更细分的刻度
    y_min, y_max = min(exec_times), max(exec_times)
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
    elif args.input:
        thread_nums, exec_times = read_results_file(args.input)
    else:
        parser.print_help(); return

    plot(thread_nums, exec_times, baseline=args.baseline, title=args.title, output=args.output)

if __name__ == '__main__':
    main()
