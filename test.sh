#!/bin/bash

# ====== 参数设置区 ======
ARGS="64 16 100000 16"   # 测试参数，可在此修改
REPEAT=5                 # 测试次数，可在此修改
# ========================

# 进入脚本所在目录（base）
cd "$(dirname "$0")"

# 增量编译
echo "开始增量编译 build_release ..."
cd build_release || { echo "build_release 目录不存在！"; exit 1; }
make -j || { echo "编译失败！"; exit 1; }
cd bin || { echo "bin 目录不存在！"; exit 1; }
echo "编译完成。"

# 检查 put_obs 是否存在
if [ ! -x ./put_obs ]; then
    echo "put_obs 可执行文件不存在或不可执行！"
    exit 1
fi

total_time=0
success_count=0

echo "开始重复测试 put_obs $ARGS（共 $REPEAT 次）..."
for ((i=1; i<=REPEAT; i++)); do
    echo "第 $i 次测试："
    output=$(./put_obs $ARGS)
    status=$?
    echo "$output"
    # 从输出中提取 TOTAL: x.xxxx s
    time_str=$(echo "$output" | grep -Eo "TOTAL: [0-9.]+ s" | grep -Eo "[0-9.]+" | head -n1)
    if [ -n "$time_str" ]; then
        # printf "本次用时：%s 秒\n" "$time_str"
        total_time=$(echo "$total_time + $time_str" | bc)
        ((success_count++))
    else
        echo "未能从输出中提取用时！"
    fi
    if [ $status -ne 0 ]; then
        echo "第 $i 次测试失败！"
    else
        echo "第 $i 次测试成功。"
    fi
    echo "-----------------------------"
done

if [ $success_count -gt 0 ]; then
    avg_time=$(echo "scale=4; $total_time / $success_count" | bc)
    printf "平均用时：%.4f 秒\n" "$avg_time"
    echo "$avg_time" >> ../../avg_time.txt
else
    echo "未能统计到任何用时，无法计算平均值。"
fi
echo "所有测试完成。"