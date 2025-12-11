# GRTC Retention 功能说明

## 概述

本项目实现了基于 nRF54L15 GRTC（Global Real-Time Counter）的 UTC 时间管理，**支持软件复位后时间戳保持**，无需使用 NVS 存储。

## 工作原理

### GRTC KEEPRUNNING 机制

nRF54L15 的 GRTC 外设支持 **KEEPRUNNING** 寄存器，可以请求 GRTC 在系统复位期间保持运行：

```c
/* 启用 GRTC retention */
nrf_grtc_sys_counter_active_state_request_set(grtc, true);
```

### 特性

1. **硬件支持**：nRF54L15 的 GRTC 有专门的 KEEPRUNNING 寄存器
2. **自动启用**：调用 `utc_time_calibrate_unix()` 时自动启用 retention
3. **软件复位后保持**：GRTC 计数器在 `sys_reboot(SYS_REBOOT_WARM)` 后继续运行
4. **无需 NVS**：不使用 Flash 存储，避免写入损耗

## 实现细节

### 1. GRTC Retention 启用

```c
static void grtc_retention_enable(void)
{
#if NRF_GRTC_HAS_KEEPRUNNING
    NRF_GRTC_Type *grtc = (NRF_GRTC_Type *)DT_REG_ADDR(GRTC_NODE);
    
    /* Request GRTC to stay active during system off/reset */
    nrf_grtc_sys_counter_active_state_request_set(grtc, true);
    
    LOG_INF("GRTC retention enabled - counter will persist through soft reset");
#endif
}
```

### 2. 校准时自动启用

```c
void utc_time_calibrate(uint64_t utc_timestamp_us)
{
    uint64_t grtc_time = z_nrf_grtc_timer_read();
    utc_offset = (int64_t)utc_timestamp_us - (int64_t)grtc_time;
    calibrated = true;
    
    /* Enable GRTC retention to preserve time through reset */
    grtc_retention_enable();
    
    LOG_INF("UTC time calibrated");
    LOG_INF("GRTC retention: %s", grtc_retention_check() ? "enabled" : "disabled");
}
```

### 3. 状态检查

```c
bool utc_time_retention_active(void)
{
#if NRF_GRTC_HAS_KEEPRUNNING
    NRF_GRTC_Type *grtc = (NRF_GRTC_Type *)DT_REG_ADDR(GRTC_NODE);
    return nrf_grtc_sys_counter_active_state_request_check(grtc);
#else
    return false;
#endif
}
```

## API 使用

### 启用 Retention（自动）

```c
// 校准时会自动启用 retention
utc_time_calibrate_unix(1733918400);  // 2024-12-11 12:00:00 UTC
```

### 手动启用

```c
// 也可以手动启用
utc_time_enable_retention();
```

### 检查状态

```c
if (utc_time_retention_active()) {
    printk("GRTC will persist through soft reset\n");
}
```

### 获取时间

```c
// 复位后 GRTC 继续计数，只需重新设置 offset
uint64_t time_sec = utc_time_get_sec();
```

## 复位后恢复策略

### 场景 1：仅软件复位，不需要重新校准

```c
void main(void) {
    // 检查 GRTC 是否已在运行
    uint64_t grtc_value = z_nrf_grtc_timer_read();
    
    if (grtc_value > 1000000ULL) {
        LOG_INF("GRTC persisted from previous session: %llu us", grtc_value);
        
        // 方案 1：从 retention RAM 恢复 offset
        // utc_offset = restore_from_retention();
        
        // 方案 2：从简单的 GPREGRET 寄存器恢复（如果 offset 较小）
        // utc_offset = nrf_power_gpregret_get();
    } else {
        LOG_INF("GRTC freshly started, needs calibration");
        utc_time_calibrate_unix(get_time_from_network());
    }
}
```

### 场景 2：使用 Retention RAM 保存 offset

```c
/* Device Tree overlay */
&gpregret {
    utc_offset: utc_offset@0 {
        compatible = "zephyr,retention";
        status = "okay";
        reg = <0x0 0x8>;  // 8 bytes for int64_t offset
    };
};

/* 代码 */
#include <zephyr/retention/retention.h>

static int64_t utc_offset_retained __attribute__((section(".retention")));

void utc_time_calibrate(uint64_t utc_timestamp_us)
{
    uint64_t grtc_time = z_nrf_grtc_timer_read();
    utc_offset_retained = (int64_t)utc_timestamp_us - (int64_t)grtc_time;
    
    grtc_retention_enable();
}
```

## 测试方法

### 1. 编译并烧录

```bash
cd nrf_grtc_timer
west build -b nrf54l15dk/nrf54l15/cpuapp --pristine
west flash
```

### 2. 观察输出

```
[00:00:00.123,456] <inf> utc_time_demo: === Example 0: Check GRTC Status ===
[00:00:00.123,789] <inf> utc_time_demo: GRTC raw counter: 123456 us
[00:00:00.124,012] <inf> utc_time_demo: GRTC retention: INACTIVE
[00:00:00.124,234] <inf> utc_time_demo: >>> GRTC appears to be freshly started

[00:00:01.000,000] <inf> utc_time: UTC time calibrated
[00:00:01.000,123] <inf> utc_time: GRTC retention enabled
[00:00:01.000,234] <inf> utc_time_demo: >>> GRTC retention is now: ENABLED
```

### 3. 软件复位测试

在串口终端中触发复位：

```bash
# 方法 1：nRF Connect Terminal
reset

# 方法 2：通过代码
sys_reboot(SYS_REBOOT_WARM);
```

### 4. 验证 retention

复位后观察 GRTC 计数器：

```
[00:00:00.000,123] <inf> utc_time_demo: GRTC raw counter: 5234567890 us
[00:00:00.000,234] <inf> utc_time_demo: GRTC retention: ACTIVE
[00:00:00.000,345] <inf> utc_time_demo: >>> GRTC has been running
```

**GRTC 值远大于 0** 说明计数器从上次会话持续运行！

## 优势

### vs NVS 存储

| 特性 | GRTC Retention | NVS 存储 |
|------|---------------|----------|
| Flash 写入 | ❌ 无 | ✅ 有（损耗） |
| 复位后可用 | ✅ 立即 | ⏱️ 需要读取 |
| 精度 | ✅ 1μs | ⚠️ 依赖保存频率 |
| 实现复杂度 | ✅ 简单 | ⚠️ 需要 NVS 子系统 |
| 硬件依赖 | ⚠️ 需要 GRTC KEEPRUNNING | ✅ 通用 |

### vs RTC

| 特性 | GRTC | RTC |
|------|------|-----|
| 频率 | ✅ 1 MHz | ❌ 32.768 kHz |
| 精度 | ✅ 1 μs | ❌ ~30 μs |
| 位宽 | ✅ 64-bit | ❌ 24-bit |
| Retention | ✅ KEEPRUNNING | ⚠️ 依赖电源域 |

## 注意事项

1. **仅软件复位有效**：
   - ✅ `sys_reboot(SYS_REBOOT_WARM)` - 有效
   - ✅ 看门狗复位 - 有效
   - ❌ 掉电复位 - 无效
   - ❌ 硬件复位按钮 - 无效

2. **仅 offset 丢失**：
   - GRTC 计数器保持运行
   - 但 RAM 中的 `utc_offset` 会丢失
   - 需要使用 retention RAM 或 GPREGRET 保存 offset

3. **功耗考虑**：
   - GRTC KEEPRUNNING 会略微增加待机功耗
   - 如果需要极低功耗，考虑在进入 System OFF 前禁用

## 完整示例输出

```
[00:00:00.000,000] <inf> utc_time_demo: UTC Time Management Demo Starting...
[00:00:00.000,123] <inf> utc_time_demo: === Example 0: Check GRTC Status ===
[00:00:00.000,234] <inf> utc_time_demo: GRTC raw counter: 123456 us
[00:00:00.000,345] <inf> utc_time_demo: GRTC retention: INACTIVE
[00:00:00.000,456] <inf> utc_time_demo: >>> GRTC appears to be freshly started

[00:00:01.000,000] <inf> utc_time: UTC time calibrated
[00:00:01.000,111] <inf> utc_time: GRTC time: 1000000 us
[00:00:01.000,222] <inf> utc_time: UTC time:  1733918400000000 us
[00:00:01.000,333] <inf> utc_time: Offset:    1733917400000000 us
[00:00:01.000,444] <inf> utc_time: GRTC retention enabled
[00:00:01.000,555] <inf> utc_time: GRTC retention: enabled
[00:00:01.000,666] <inf> utc_time_demo: >>> Calibrated with Unix timestamp: 1733918400
[00:00:01.000,777] <inf> utc_time_demo: >>> GRTC retention is now: ENABLED

[00:00:11.000,000] <inf> utc_time_demo: *** IMPORTANT: GRTC Retention is ENABLED ***
[00:00:11.000,111] <inf> utc_time_demo: >>> After software reset, GRTC will continue counting
[00:00:11.000,222] <inf> utc_time_demo: >>> Only offset needs to be restored to maintain UTC time
```

## 相关文件

- `src/utc_time.c` - UTC 时间管理实现（含 GRTC retention）
- `src/utc_time.h` - API 头文件
- `src/main.c` - 演示程序
- `prj.conf` - 项目配置

## 参考文档

- nRF54L15 Product Specification - GRTC chapter
- Zephyr RTOS - System Timer API
- Nordic HAL - `nrf_grtc.h`
