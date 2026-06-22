# 汇川机械臂连接后处理流程

本文档记录 GUI 连接汇川控制器后必须执行的操作、对应 SDK 函数、当前工程中的处理函数，以及常见运行日志的判断方法。

## 前置条件

上位机要执行写变量、写 IO、使能、启动程序等控制类操作前，示教器或 InoRobotLab 必须先把控制设备切换为“远程以太网客户端”。

汇川 SDK 接口大致分两类：

- 监控类接口：连接成功后即可调用，例如读取变量、读取状态、读取当前位置。
- 控制类接口：不仅要连接成功，还需要获取控制许可，并且当前 SDK 连接需要具备足够用户权限。

## 连接后执行顺序

1. 加载连接配置和当前品牌。
   - UI 入口：`MainWindow::onConnectClicked()`
   - 文件：`mainwindow.cpp`
   - 说明：读取 `robot_connection.ini`，并设置当前品牌为 `RobotBrand::Huichuan`。

2. 调用汇川 SDK 建立以太网连接。
   - 适配层入口：`robot_arm_actions::connect(...)`
   - 汇川后端入口：`robot_backend::huichuan::connect_robot(...)`
   - 文件：`robot_sdk/cpp/huichuan_backend.cpp`
   - SDK 函数：`IMC100_Init_ETH(ipAddr, port, timeoutSeconds, comId)`
   - 当前参数：`timeoutSeconds=5`，`comId=0`
   - 注意：IP 地址按网络序打包，已与可连接成功的 `FurPoInter` 实现对齐。

3. GUI 完成连接状态更新。
   - UI 入口：`MainWindow::finishConnect(...)`
   - 文件：`mainwindow.cpp`
   - 说明：设置已连接状态、启用按钮、更新状态栏。

4. 获取控制许可。
   - 适配层入口：`robot_login_admin_user()`
   - 汇川后端入口：`robot_backend::huichuan::login_admin_user()`
   - 内部辅助函数：`ensureControlPermit(...)`
   - 文件：`robot_sdk/cpp/huichuan_backend.cpp`
   - SDK 函数：
     - `IMC100_CurPermit(&owner, &ip, &port, comId)`
     - `IMC100_AcqPermit(0, comId)`
     - 如果普通申请失败或已有占用，再调用 `IMC100_AcqPermit(1, comId)`

5. 登录汇川用户权限。
   - 汇川后端入口：`robot_backend::huichuan::login_admin_user()`
   - 文件：`robot_sdk/cpp/huichuan_backend.cpp`
   - SDK 函数：
     - 登录前：`IMC100_CurUserType(&type, comId)`
     - 登录：`IMC100_UserLogin(2, "000000", comId)`
     - 登录后：`IMC100_CurUserType(&type, comId)`
   - 当前配置：`type=2`，密码 `000000`
   - 运行日志会显示登录前用户等级、登录返回码、登录后用户等级，以及控制许可状态。

6. 启动状态轮询和界面刷新。
   - UI 入口：`MainWindow::finishConnect(...)`
   - 文件：`mainwindow.cpp`
   - 调用：`pollRobotState()`、`refreshStatusSummary()`、`m_updateTimer->start()`

## 全局变量写入流程

1. 用户点击写入变量。
   - UI 入口：`MainWindow::onSetGlobalVariant()`
   - 文件：`mainwindow.cpp`

2. 变量名解析并路由到汇川后端。
   - 适配层入口：`robot_set_global_variant(...)`
   - 汇川后端入口：`robot_backend::huichuan::set_global_variant(...)`
   - 文件：`robot_sdk/cpp/huichuan_backend.cpp`
   - 变量名映射：
     - `GBnnn` 或 `Bnnn` -> `IMC100_Set_B(index, value, comId)`
     - `GRnnn`、`GInnn`、`Rnnn` 或 `Innn` -> `IMC100_Set_R(index, value, comId)`
     - `GDnnn` 或 `Dnnn` -> `IMC100_Set_D(index, value, comId)`

3. 写入前再次检查控制许可。
   - 辅助函数：`ensureControlPermit(...)`
   - 原因：连接成功后，控制许可仍可能被其他以太网客户端改变。

4. 调用实际 SDK 写入函数。
   - SDK 函数：`IMC100_Set_B`、`IMC100_Set_R` 或 `IMC100_Set_D`
   - 如果失败，`robot_last_global_variant_error()` 会返回诊断信息，UI 会追加到运行日志中。

## 运行日志判断

连接成功后，理想的自动登录日志类似：

```text
Huichuan admin login: permitBeforeResult=0, ownerBefore=0, permitResult=0, permitAfterResult=0, ownerAfterPermit=1, beforeResult=0, beforeUserType=0, loginType=2, loginResult=0, afterResult=0, afterUserType=2
```

关键字段含义：

- `permitResult=0`：控制许可获取成功。
- `ownerAfterPermit=1`：当前 GUI 这个以太网客户端已经拥有控制许可。
- `loginResult=0`：用户登录成功。
- `afterUserType=2`：当前 SDK 连接已经切到配置的管理员/编辑用户等级。

## 常见错误码

- `-24`：`without permit`，当前以太网设备没有控制许可。需要先获取控制许可，再登录或写入。
- `-25`：`ETH without authorization`，当前控制设备不是远程以太网客户端。需要在示教器或 InoRobotLab 中切换控制设备。
- `-27`：`low user grade`，当前用户权限等级不够。需要调用 `IMC100_UserLogin` 登录到编辑/管理员及以上权限。
- `-28`：`permit occupied`，控制许可已被其他以太网客户端占用。需要关闭其他客户端或强制获取控制许可。

## 相关源码文件和函数

`mainwindow.cpp`

- `MainWindow::onConnectClicked()`
- `MainWindow::finishConnect(...)`
- `MainWindow::onSetGlobalVariant()`

`robot_sdk/cpp/huichuan_backend.cpp`

- `connect_robot(...)`
- `login_admin_user()`
- `ensureControlPermit(...)`
- `set_global_variant(...)`
- `last_global_variant_error()`

`robot_sdk/cpp/robot_adapter.cpp`

- `robot_login_admin_user()`
- `robot_set_global_variant(...)`
- `robot_last_global_variant_error()`

`robot_sdk/cpp/interface/nrc_interface.h`

- GUI 使用的通用适配接口声明。

`robot_sdk/cpp/backend_bridge.h`

- 汇川后端专用函数声明。
