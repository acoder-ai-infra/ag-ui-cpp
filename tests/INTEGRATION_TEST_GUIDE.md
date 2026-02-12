# AG-UI 集成测试指南

本指南说明如何运行与Mock服务器交互的集成测试。

## 📋 集成测试概述

集成测试文件：`tests/test_integration_with_server.cpp`

该测试包含3个主要测试场景：

1. **HTTP客户端与服务器交互** - 测试HTTP请求/响应和SSE流
2. **HttpAgent与服务器交互** - 测试Agent运行和事件订阅
3. **中间件与服务器交互** - 测试中间件拦截和处理事件

## 🚀 快速开始

### 步骤1：启动Mock服务器

```bash
# 在终端1中启动Mock服务器
cd /Users/dangai/workspace/code/temp_code/ag-ui_test
python3 tests/mock_server/mock_ag_server.py
```

服务器启动后会显示：
```
======================================
  AG-UI Mock Server
======================================
Server running on http://localhost:8080
Press Ctrl+C to stop
```

### 步骤2：编译集成测试

```bash
# 在终端2中编译
cd /Users/dangai/workspace/code/temp_code/ag-ui_test/build
cmake --build . --config Debug --target test_integration_with_server
```

### 步骤3：运行集成测试

```bash
# 运行集成测试
./Debug/test_integration_with_server
```

测试会提示您按Enter键继续，确保Mock服务器已启动后按Enter。

## 📊 测试场景详解

### 测试1：HTTP客户端与服务器交互

测试内容：
- **1.1 健康检查端点** - GET /health
- **1.2 获取场景列表** - GET /scenarios  
- **1.3 SSE流式响应** - POST /api/agent/run

预期输出：
```
[INTEGRATION_TEST] 集成测试1: HTTP客户端与Mock服务器交互
[INTEGRATION_TEST] 测试1.1: 健康检查端点
[INTEGRATION_TEST] 收到健康检查响应
✓ 健康检查返回成功状态
✓ 状态码为200
[INTEGRATION_TEST] 响应内容: {"status":"ok","message":"Mock AG-UI Server is running"}

[INTEGRATION_TEST] 测试1.2: 获取场景列表
[INTEGRATION_TEST] 收到场景列表响应
✓ 场景列表返回成功
[INTEGRATION_TEST] 场景列表: {...}

[INTEGRATION_TEST] 测试1.3: SSE流式响应
[INTEGRATION_TEST] 收到SSE事件 #1: ...
[INTEGRATION_TEST] 收到SSE事件 #2: ...
[INTEGRATION_TEST] SSE流完成
✓ 收到了SSE事件
[INTEGRATION_TEST] 总共收到 X 个事件
```

### 测试2：HttpAgent与服务器交互

测试内容：
- **2.1 运行simple_text场景** - 测试基本文本生成
- **2.2 运行with_thinking场景** - 测试带思考过程的生成

预期输出：
```
[INTEGRATION_TEST] 集成测试2: HttpAgent与Mock服务器交互
[INTEGRATION_TEST] 测试2.1: 运行simple_text场景
[INTEGRATION_TEST] 订阅者: RUN_STARTED - runId=...
[INTEGRATION_TEST] 订阅者: TEXT_MESSAGE_START - messageId=...
[INTEGRATION_TEST] 订阅者: TEXT_MESSAGE_CONTENT - delta=...
[INTEGRATION_TEST] 订阅者: TEXT_MESSAGE_END
[INTEGRATION_TEST] 订阅者: RUN_FINISHED
[INTEGRATION_TEST] Agent运行成功
✓ 收到RUN_STARTED事件
✓ 收到TEXT_MESSAGE_START事件
✓ 收到TEXT_MESSAGE_CONTENT事件
✓ 收到TEXT_MESSAGE_END事件
✓ 收到RUN_FINISHED事件
[INTEGRATION_TEST] 完整内容: ...
```

### 测试3：中间件与服务器交互

测试内容：
- **3.1 事件计数中间件** - 统计捕获的事件数量
- **3.2 事件过滤中间件** - 过滤THINKING事件

预期输出：
```
[INTEGRATION_TEST] 集成测试3: 中间件与Mock服务器交互
[INTEGRATION_TEST] 测试3.1: 事件计数中间件
[INTEGRATION_TEST] 中间件测试场景完成
✓ 中间件捕获了事件
✓ 中间件捕获了TEXT事件
[INTEGRATION_TEST] 总事件数: X
[INTEGRATION_TEST] TEXT事件数: Y

[INTEGRATION_TEST] 测试3.2: 事件过滤中间件（with_thinking场景）
[INTEGRATION_TEST] 过滤测试场景完成
[INTEGRATION_TEST] TEXT事件数: X
[INTEGRATION_TEST] THINKING事件数: Y
✓ 捕获了TEXT事件
```

## 🔧 Mock服务器场景

集成测试使用以下Mock服务器场景：

### simple_text
简单文本生成场景，包含：
- RUN_STARTED
- TEXT_MESSAGE_START
- TEXT_MESSAGE_CONTENT (多次)
- TEXT_MESSAGE_END
- RUN_FINISHED

### with_thinking
带思考过程的场景，包含：
- RUN_STARTED
- THINKING_TEXT_MESSAGE_START
- THINKING_TEXT_MESSAGE_CONTENT (多次)
- THINKING_TEXT_MESSAGE_END
- TEXT_MESSAGE_START
- TEXT_MESSAGE_CONTENT (多次)
- TEXT_MESSAGE_END
- RUN_FINISHED

更多场景请参考：`tests/mock_server/README.md`

## 📝 测试配置

### 超时设置

集成测试的超时时间为60秒（在CMakeLists.txt中配置）：

```cmake
set_tests_properties(IntegrationTests PROPERTIES
    TIMEOUT 60
    LABELS "integration;server"
)
```

### 延迟配置

可以通过请求参数调整事件延迟：

```cpp
params.input.delay_ms = 100;  // 每个事件延迟100ms
```

## ⚠️ 常见问题

### Q1: 连接失败

**错误信息：**
```
⚠️  无法连接到Mock服务器，请确保服务器已启动
```

**解决方案：**
1. 确认Mock服务器正在运行
2. 检查端口8080是否被占用
3. 确认防火墙设置

### Q2: 测试超时

**原因：**
- Mock服务器响应慢
- 网络延迟
- 事件延迟设置过大

**解决方案：**
1. 减小delay_ms参数
2. 增加测试超时时间
3. 检查Mock服务器日志

### Q3: 事件未收到

**原因：**
- 订阅者未正确注册
- 事件类型不匹配
- 中间件过滤了事件

**解决方案：**
1. 检查订阅者实现
2. 确认事件类型正确
3. 检查中间件配置

## 🎯 最佳实践

### 1. 测试前准备

- ✅ 确保Mock服务器已启动
- ✅ 检查端口8080可用
- ✅ 编译最新的测试代码

### 2. 测试执行

- ✅ 按顺序运行测试
- ✅ 观察控制台输出
- ✅ 检查所有断言通过

### 3. 测试后清理

- ✅ 停止Mock服务器（Ctrl+C）
- ✅ 检查测试日志
- ✅ 清理临时文件

## 📚 相关文档

- **Mock服务器文档**: `tests/mock_server/README.md`
- **测试总结**: `tests/NEW_TESTS_SUMMARY.md`
- **完整测试指南**: `COMPLETE_TEST_GUIDE.md`

## 🔄 持续集成

### 在CI/CD中运行

```bash
# 启动Mock服务器（后台）
python3 tests/mock_server/mock_ag_server.py &
SERVER_PID=$!

# 等待服务器启动
sleep 2

# 运行集成测试
./Debug/test_integration_with_server

# 停止Mock服务器
kill $SERVER_PID
```

### Docker环境

```dockerfile
# 在Docker中运行
FROM ubuntu:22.04

# 安装依赖
RUN apt-get update && apt-get install -y \
    python3 \
    cmake \
    g++ \
    libcurl4-openssl-dev

# 复制代码
COPY . /app
WORKDIR /app

# 编译
RUN mkdir build && cd build && \
    cmake -DBUILD_TESTS=ON .. && \
    cmake --build .

# 运行测试
CMD ["bash", "-c", "python3 tests/mock_server/mock_ag_server.py & sleep 2 && ./build/Debug/test_integration_with_server"]
```

## 🎉 总结

集成测试验证了：

✅ HTTP客户端能正确发送请求和接收响应
✅ HttpAgent能正确运行并触发事件
✅ 订阅者能正确接收和处理事件
✅ 中间件能正确拦截和处理事件
✅ SSE流式响应能正确解析

**测试覆盖率：**
- HTTP客户端：3个测试场景
- HttpAgent：2个测试场景
- 中间件：2个测试场景

**总计：7个集成测试场景**

祝测试顺利！🚀
