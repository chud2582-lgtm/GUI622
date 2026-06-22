## [2026-06-18] - Fix Huichuan write variable config parsing
### Changed
- **Files**: `mainwindow.cpp`
- **Details**:
  - Fixed parsing of `[global_variable/N]` groups in `robot_connection_huichuan.ini`.
  - Huichuan start can now find the configured `mode=write` global variable.

## [2026-06-18] - Read Huichuan active program from main task first
### Changed
- **Files**: `robot_sdk/cpp/huichuan_backend.cpp`
- **Details**:
  - Changed Huichuan current program query to try `IMC100_Get_TaskPrgPath(0, ...)` before task `1`.
  - This matches the working `FurPoInter` implementation and avoids `SUCCESS` with an empty task-1 path.

## [2026-06-18] - Distinguish empty Huichuan active program
### Changed
- **Files**: `mainwindow.cpp`
- **Details**:
  - Split Huichuan active program read failures from successful SDK calls that return an empty program name.
  - Empty active program logs now include the configured program name for comparison.

## [2026-06-18] - Clarify Huichuan active program log
### Changed
- **Files**: `mainwindow.cpp`
- **Details**:
  - Updated Huichuan start logs to explicitly show both the teach pendant active program name and the configured program name.

## [2026-06-18] - Validate Huichuan active program before start
### Changed
- **Files**: `mainwindow.cpp`
- **Details**:
  - Huichuan start now compares the teach pendant active program with `active_program/name` from `robot_connection_huichuan.ini`, ignoring file extension.
  - If programs differ, the UI shows a warning and records the mismatch in the runtime log.
  - When the program matches, the selected six-digit workpiece ID is written to the configured `mode=write` global variable before starting.

## [2026-06-18] - Design Huichuan variable action config
### Changed
- **Files**: `robot_connection_huichuan.ini`
- **Details**:
  - Added an `active_program` section for the teach pendant active program name.
  - Added sample `global_variable/*` sections with variable name, read/write mode, and value fields.

## [2026-06-18] - Make disabled action buttons gray
### Changed
- **Files**: `mainwindow.cpp`
- **Details**:
  - Added explicit disabled styles for success, danger, warning, and primary buttons.
  - Disabled job/action buttons now render with gray background, border, and text.

## [2026-06-18] - Gate job buttons by workpiece selection
### Changed
- **Files**: `mainwindow.cpp`, `mainwindow.h`
- **Details**:
  - Disabled start/pause/stop/reset buttons by default.
  - Enabled the four job buttons only after a workpiece is selected and the controller is connected.

## [2026-06-17] - Localize Huichuan flow document
### Changed
- **Files**: `HUICHUAN_CONNECTION_FLOW.md`
- **Details**:
  - Rewrote the Huichuan post-connect flow document in Chinese.
  - Kept SDK function names, code handler names, and error codes for maintenance reference.

## [2026-06-17] - Document Huichuan post-connect flow
### Changed
- **Files**: `HUICHUAN_CONNECTION_FLOW.md`
- **Details**:
  - Added a maintenance guide for Huichuan connection, control permit acquisition, admin login, and global variable write flow.
  - Documented related SDK calls, current code handlers, and common runtime error codes.

## [2026-06-17] - Acquire Huichuan control permit before admin login
### Changed
- **Files**: `robot_sdk/cpp/huichuan_backend.cpp`
- **Details**:
  - Changed automatic Huichuan admin login to acquire `IMC100_AcqPermit` before calling `IMC100_UserLogin`.
  - Added control-permit before/after details to the admin login runtime log.

## [2026-06-17] - Auto login Huichuan admin user after connect
### Changed
- **Files**: `mainwindow.cpp`, `robot_sdk/cpp/backend_bridge.h`, `robot_sdk/cpp/huichuan_backend.cpp`, `robot_sdk/cpp/interface/nrc_interface.h`, `robot_sdk/cpp/robot_adapter.cpp`
- **Details**:
  - Added automatic Huichuan `IMC100_UserLogin(type=2, password=000000)` after a successful connection.
  - Runtime logs now show user type before login, login return code, and user type after login.

## [2026-06-17] - Explain Huichuan low user grade write failures
### Changed
- **Files**: `robot_sdk/cpp/huichuan_backend.cpp`
- **Details**:
  - Added `IMC100_CurUserType` details to Huichuan global variable write failure diagnostics.
  - Added a specific hint for SDK code `-27` indicating that write APIs require edit/admin-or-higher user level.

## [2026-06-17] - Expand Huichuan control permit diagnostics
### Changed
- **Files**: `robot_sdk/cpp/huichuan_backend.cpp`
- **Details**:
  - Added raw `IMC100_AcqPermit(0)` and `IMC100_AcqPermit(1)` return codes to global variable write diagnostics.
  - Added before/after `IMC100_CurPermit` owner details to identify whether control permission is unavailable, rejected, or held by another endpoint.

## [2026-06-17] - Add Huichuan global variable write diagnostics
### Changed
- **Files**: `mainwindow.cpp`, `robot_sdk/cpp/backend_bridge.h`, `robot_sdk/cpp/huichuan_backend.cpp`, `robot_sdk/cpp/interface/nrc_interface.h`, `robot_sdk/cpp/robot_adapter.cpp`
- **Details**:
  - Added runtime diagnostics for Huichuan global variable write failures.
  - Logs now include control-permit query/acquisition details and raw SDK write return codes when `IMC100_Set_B/R/D` cannot complete.

## [2026-06-17] - Fix Huichuan global variable write permission
### Changed
- **Files**: `robot_sdk/cpp/huichuan_backend.cpp`
- **Details**:
  - Added Huichuan control-permit acquisition before `IMC100_Set_B/R/D`.
  - Matched `FurPoInter` behavior so writable global variables can be updated after a successful connection.

## [2026-06-17] - Align Huichuan connection with FurPoInter
### Changed
- **Files**: `robot_sdk/cpp/huichuan_backend.cpp`
- **Details**:
  - Matched Huichuan IP packing with the working `FurPoInter` implementation by using network-order IPv4 encoding.
  - Matched the `IMC100_Init_ETH` timeout argument to the working project's 5-second SDK timeout value.

## [2026-06-17] - Add global IO section to global variable page
### Changed
- **Files**: `mainwindow.cpp`
- **Details**:
  - Moved the existing DI/DO read and DO write controls into the global variable page under a new "全局 IO" section.
  - Enabled IO controls together with the connection state so the page now exposes both global variable and IO operations in one place.

## [2026-06-17] - Add TCP probe for connection failures
### Changed
- **Files**: `CMakeLists.txt`, `mainwindow.cpp`, `mainwindow.h`
- **Details**:
  - Added a short TCP endpoint probe after SDK connection failures.
  - Runtime logs now distinguish unreachable controller ports from reachable TCP ports where SDK protocol initialization still fails.

## [2026-06-17] - Add Huichuan connection failure details
### Changed
- **Files**: `mainwindow.cpp`, `mainwindow.h`, `robot_sdk/cpp/backend_bridge.h`, `robot_sdk/cpp/huichuan_backend.cpp`, `robot_sdk/cpp/huichuan_arm_actions.cpp`, `robot_sdk/cpp/robot_arm_actions.cpp`, `robot_sdk/cpp/robot_arm_actions.h`, `robot_sdk/cpp/robot_arm_actions_backend.h`
- **Details**:
  - Added detailed Huichuan connection failure diagnostics to the runtime log and failure dialog.
  - Record invalid IP/port inputs, stale connection close failures, and raw `IMC100_Init_ETH` SDK return codes with actionable hints.

## [2026-06-16] - Document Huichuan global variable calls
### Changed
- **Files**: `mainwindow.cpp`
- **Details**:
  - Added a note on the global variable page describing Huichuan B/R/D SDK mappings.

## [2026-06-16] - Add global variable page
### Changed
- **Files**: `mainwindow.cpp`, `workpieceselectionwidget.cpp`, `workpieceselectionwidget.h`
- **Details**:
  - Added a "全局变量" page at the same page-switch level as "工件选择" and "工件缩略图".
  - Reused the existing global variable input, value, read, and write controls on the new page.

## [2026-06-16] - Improve global variable read/write
### Changed
- **Files**: `mainwindow.cpp`, `mainwindow.h`
- **Details**:
  - Added validation for user-entered global variable names before read/write.
  - Added clearer runtime logs for preparing, reading, and writing global variables.
  - Disabled global variable read/write buttons until the controller is connected.

## [2026-06-16] - Save runtime logs by date
### Changed
- **Files**: `mainwindow.cpp`
- **Details**:
  - Runtime log messages are now appended to `logs/YYYY-MM-DD.txt`.
  - File logs use plain text while the GUI log keeps its existing styled HTML display.

## [2026-06-16] - Fix brand-specific start behavior
### Changed
- **Files**: `mainwindow.cpp`, `robot_sdk/cpp/huichuan_arm_actions.cpp`
- **Details**:
  - Huichuan start now queries the active program on the teach pendant before issuing the run command.
  - Yinglian start still uses the "作业名称" field directly.

## [2026-06-16] - Rename Yinglian display text
### Changed
- **Files**: `mainwindow.cpp`
- **Details**:
  - Changed the user-facing robot brand label from `英联` to `盈联`.
  - Kept the internal `yinglian` configuration key and SDK namespace unchanged.

## [2026-06-16] - Add brand-specific connection settings
### Changed
- **Files**: `robot_connection.ini`, `mainwindow.cpp`, `mainwindow.h`
- **Details**:
  - Added brand-specific connection groups for Huichuan and Yinglian controllers.
  - Configured Yinglian with IP `192.168.3.15` and port `6003`.
  - Updated GUI connection loading to use the currently selected brand while keeping the old `connection/ip` and `connection/port` keys as fallback.

## [2026-06-16] - Split robot arm actions by brand
### Changed
- **Files**: `mainwindow.cpp`, `mainwindow.h`, `CMakeLists.txt`, `robot_sdk/cpp/robot_arm_actions*`, `robot_sdk/cpp/*_arm_actions.cpp`, `robot_sdk/cpp/robot_adapter.cpp`
- **Details**:
  - Added a unified robot arm action interface for connect, enable, disconnect, and job start/pause/continue/stop/reset.
  - Moved Huichuan and Yinglian implementations of those arm actions into separate brand-specific source files.
  - Updated the GUI to call the unified arm action interface while keeping IO, variables, status polling, and other non-arm GUI logic on the existing common framework.

## [2026-06-16] - Fix malformed UI QString literals
### Fixed
- **Files**: `mainwindow.cpp`
- **Details**:
  - Restored damaged Chinese UI strings that broke `QStringLiteral` macro parsing.
  - Fixed job-control, status, IO table, and disconnect log text after the robot brand adapter changes.

## [2026-06-16] - Add robot brand selection
### Changed
- **Files**: `mainwindow.cpp`, `mainwindow.h`, `CMakeLists.txt`, `robot_sdk/cpp/*`
- **Details**:
  - Added a robot brand selector to the connection panel for Huichuan and Yinglian arms.
  - Added a unified `robot_*` adapter layer so GUI code can switch brands without calling brand-specific SDK APIs directly.
  - Split Huichuan IMC100 and Yinglian NRC SDK calls into separate backend files and linked both bundled x64 SDK libraries.
  - Persisted the selected brand in `robot_connection.ini` under `connection/brand`.

## [2026-06-16] - Adapt GUI to IMC100 SDK
### Changed
- **Files**: `CMakeLists.txt`, `mainwindow.cpp`, `robot_sdk/cpp/robot_adapter.cpp`, `robot_sdk/cpp/interface/nrc_interface.h`, `robot_sdk/cpp/interface/nrc_io.h`, `robot_sdk/cpp/interface/nrc_job_operate.h`, `robot_sdk/cpp/parameter/nrc_define.h`
- **Details**:
  - Replaced the old `nrc_host` linkage path with the bundled `robot_sdk/x64/IMC100API.lib` and `IMC100API.dll`.
  - Added a local compatibility layer so the existing GUI can keep calling `connect_robot`, job control, IO, and variable APIs.
  - Mapped global variables to the IMC100 B/R/D register APIs and refreshed IO reads from the live SDK instead of placeholder values.
  - Loaded variable presets from `global_var_presets.txt` to align the UI with the existing preset list.

## [2026-06-11] - Fix connection buttons overlap
### Fixed
- **Files**: `mainwindow.cpp`
- **Details**:
  - Increased the connection settings panel height for enlarged 24px button text.
  - Increased vertical spacing between connection rows to keep buttons clear of the job-name row.
  - Top-aligned the connect, enable, and disconnect buttons in the first row.

## [2026-06-11] - Match selector input text size to buttons
### Changed
- **Files**: `workpieceselectionwidget.cpp`
- **Details**:
  - Set the "品类" label text to 24px to match button text.
  - Set the category combo box and popup item text to 24px.
  - Set the workpiece ID input text to 24px and raised selector/input minimum height to match enlarged controls.

## [2026-06-11] - Double button text size
### Changed
- **Files**: `mainwindow.cpp`, `workpieceselectionwidget.cpp`
- **Details**:
  - Set global `QPushButton` text size to 24px, twice the previous 12px base size.
  - Doubled workpiece-specific button style font sizes from 12px to 24px.
  - Increased fixed heights and widths for workpiece page-switch buttons so enlarged Chinese text remains visible.

## [2026-06-11] - Move category selector above search
### Changed
- **Files**: `workpieceselectionwidget.h`, `workpieceselectionwidget.cpp`
- **Details**:
  - Moved the workpiece category label and combo box into the right-side selection action bar.
  - Placed the category selector above the search input and search button.
  - Removed the now-empty workpiece selection page action-area member while keeping category filtering behavior connected.

## [2026-06-11] - Double search button width
### Changed
- **Files**: `workpieceselectionwidget.cpp`
- **Details**:
  - Set the workpiece search button fixed width to twice its default size hint.
  - Kept the surrounding search input and action-bar layout behavior unchanged.

## [2026-06-11] - Fix initial workpiece preview size
### Fixed
- **Files**: `workpieceselectionwidget.h`, `workpieceselectionwidget.cpp`
- **Details**:
  - Deferred preview image layout and repaint until Qt finishes the initial widget layout pass.
  - Added preview-area resize handling so the selected workpiece image is rescaled when the preview surface receives its final size.
  - Fixed the startup case where the workpiece selection page preview image appeared too small until switching pages.

## [2026-06-11] - Keep category selector and remove job status labels
### Changed
- **Files**: `workpieceselectionwidget.h`, `workpieceselectionwidget.cpp`
- **Details**:
  - Kept the workpiece category label, combo box, and category filter connection on the workpiece selection page.
  - Removed the visible "操作区" title and "当前作业" label below the category selector.
  - Removed the unused selected-job label member and update logic.

## [2026-06-11] - Restore workpiece selection action area
### Changed
- **Files**: `workpieceselectionwidget.h`, `workpieceselectionwidget.cpp`
- **Details**:
  - Restored the workpiece selection page action-area panel.
  - Restored the visible "操作区" and "当前作业" labels.
  - Kept the search input and search button in the right-side action bar under the TCP pose panel.

## [2026-06-11] - Move workpiece search controls to right action area
### Changed
- **Files**: `workpieceselectionwidget.cpp`
- **Details**:
  - Moved the workpiece ID search input and search button from the workpiece selection page action area into the right-side selection action bar.
  - Changed the selection action bar to a vertical layout so the search row appears above the favorite/select buttons under the TCP pose panel.

## [2026-06-11] - Move workpiece action buttons above run log
### Changed
- **Files**: `workpieceselectionwidget.h`, `workpieceselectionwidget.cpp`, `mainwindow.cpp`
- **Details**:
  - Exposed the workpiece favorite/select button row as a reusable action bar.
  - Removed the favorite/select button row from the workpiece selection page action area.
  - Added the action bar to the right-side panel above the "运行日志" group.

## [2026-06-11] - Reposition workpiece page tabs
### Changed
- **Files**: `mainwindow.cpp`, `workpieceselectionwidget.cpp`
- **Details**:
  - Removed the outer workpiece `QTabWidget` wrapper so the internal workpiece page switcher sits at the top of the workpiece area.
  - Changed "工件选择" and "工件缩略图" page buttons to fixed-width, left-aligned tabs.
  - Reduced the workpiece widget outer margin so the thumbnail tab aligns with the requested top-left area.

## [2026-06-11] - Add switchable workpiece pages
### Changed
- **Files**: `workpieceselectionwidget.h`, `workpieceselectionwidget.cpp`
- **Details**:
  - Converted the workpiece selection area and thumbnail area from side-by-side panels into switchable pages.
  - Added "工件选择" and "工件缩略图" page buttons for click-based switching.
  - Kept thumbnail filtering, preview selection, favorite, search, and choose behavior wired to the existing controls.
  - Expanded the thumbnail page to use the full available page width instead of a fixed narrow side panel.

## [2026-06-11] - Align connection action buttons
### Changed
- **Files**: `mainwindow.cpp`
- **Details**:
  - Set the connection, enable, and disconnect buttons to a fixed equal width.
  - Moved the connection panel content upward by reducing top margin and row spacing.

## [2026-06-11] - Move enable button into connection panel
### Changed
- **Files**: `mainwindow.cpp`
- **Details**:
  - Moved the servo enable button from job control into the connection settings panel.
  - Renamed the button to "使能" and aligned it horizontally with "连接" and "断开".
  - Set the connection, enable, and disconnect buttons to the same minimum width.
  - Placed workpiece ID and job name on the same row.

## [2026-06-11] - Fix connection panel layout overlap
### Changed
- **Files**: `mainwindow.cpp`
- **Details**:
  - Increased the connection settings panel height to avoid overlap between job name and connection buttons.
  - Put workpiece ID, job name, and progress bar on separate rows.
  - Kept the connection progress bar visible in normal state and switched it to busy mode only while connecting.

## [2026-06-11] - Move job fields into connection panel
### Changed
- **Files**: `mainwindow.cpp`
- **Details**:
  - Moved workpiece ID and job name inputs from the job control panel into the connection settings panel.
  - Shifted the connection progress bar to a lower row to avoid overlapping the connect/disconnect buttons.
  - Aligned the top control panels to the top edge.

## [2026-06-11] - Compact connection panel and remove run count
### Changed
- **Files**: `mainwindow.h`, `mainwindow.cpp`
- **Details**:
  - Reduced the connection settings panel height with a tighter one-line layout and smaller progress bar.
  - Removed the job run count input from the job control panel.
  - Removed the `job_run_times()` call before starting jobs.

## [2026-06-11] - Update connection progress UI
### Changed
- **Files**: `mainwindow.h`, `mainwindow.cpp`
- **Details**:
  - Removed the connection settings display for the config file path.
  - Added a busy progress bar under the connection controls while connecting.
  - Moved the controller connection call to a background thread so the progress indicator can animate during connection attempts.

## [2026-06-11] - Remove current program query button
### Changed
- **Files**: `mainwindow.h`, `mainwindow.cpp`
- **Details**:
  - Removed the "查询当前程序" button from the job control panel.
  - Removed the button member, click connection, and `onTopJobQueryCurrentProgram()` slot implementation.
  - Kept the current-program SDK helper used by job start validation.

## [2026-06-11] - Remove demo mode
### Changed
- **Files**: `mainwindow.h`, `mainwindow.cpp`
- **Details**:
  - Removed `m_demoMode` state and all demo-mode branches from connection, servo, job, IO, global variable, and timer logic.
  - Connection failures now keep the UI in the disconnected state instead of offering demo mode.
  - Runtime state updates now always come from the controller SDK while connected.

## [2026-06-11] - Read robot connection from config file
### Changed
- **Files**: `mainwindow.h`, `mainwindow.cpp`, `CMakeLists.txt`, `build_exe.ps1`, `build_ubuntu_arm64.sh`, `robot_connection.ini`
- **Details**:
  - Removed editable IP/port inputs from the connection panel and replaced them with read-only config and endpoint labels.
  - Added `robot_connection.ini` as the fixed source for `connection/ip` and `connection/port`.
  - Reload the connection config when the connect button is clicked, so config edits are picked up without restarting.
  - Copy `robot_connection.ini` to build and packaged output directories.

## [2026-06-10] - Adapt robot SDK integration to current robot_sdk
### Changed
- **Files**: `CMakeLists.txt`, `mainwindow.h`, `mainwindow.cpp`
- **Details**:
  - Switched the build from the removed IMC100 compatibility source to the current `robot_sdk` NRC host SDK layout.
  - Linked Windows builds against `robot_sdk/windows/.../libnrc_host.dll.a` and staged `nrc_host.dll`; added Linux `libnrc_host.so` selection for bundled x64/arm64 SDK variants.
  - Updated application includes to use `robot_sdk/cpp/...` headers directly.
  - Replaced old compatibility-only job diagnostics/current-program helpers with local UI-side helpers based on the current SDK APIs.

## [2026-06-04] - 增加伺服上电使能按钮
### 新增
- **涉及文件**: `mainwindow.h`, `mainwindow.cpp`
- **改动说明**:
  - 在作业控制区增加“伺服上电/使能”按钮。
  - 复用现有 `onServoPowerOn()` 逻辑调用 `set_servo_poweron()`，便于作业启动前完成电机使能。

## [2026-06-04] - 增加作业启动前置条件诊断日志
### 优化
- **涉及文件**: `mainwindow.cpp`, `cpp/interface/nrc_job_operate.h`, `cpp/interface/nrc_compat.cpp`
- **改动说明**:
  - 新增作业启动前置条件诊断接口，逐项检查 SDK 加载、连接状态、远程控制权、急停、系统报警、电机使能和自动模式设置。
  - 作业启动失败时输出每项检查的 Result 与原始状态细节，便于定位 `OPERATION_NOT_ALLOWED` 的具体原因。

## [2026-06-04] - 修正当前程序工程名匹配逻辑
### 修复
- **涉及文件**: `mainwindow.cpp`, `cpp/interface/nrc_compat.cpp`
- **改动说明**:
  - 将作业启动前的程序一致性校验从比较 `main.pro` 文件名改为比较当前程序路径的工程目录名，例如 `TeachProgram/NEWA/main.pro` 匹配作业 `NEWA`。
  - “查询当前程序”成功后回填工程名而不是主程序文件名。
  - 启动作业受限时输出当前程序路径和多种可能原因，避免误报为单一“程序不一致”。

## [2026-06-04] - 增加查询示教器当前程序按钮
### 新增
- **涉及文件**: `mainwindow.h`, `mainwindow.cpp`, `cpp/interface/nrc_job_operate.h`, `cpp/interface/nrc_compat.cpp`
- **改动说明**:
  - 在作业控制区增加“查询当前程序”按钮。
  - 封装 `IMC100_Get_TaskPrgPath(0, ...)` 查询主任务当前加载程序路径。
  - 查询成功后在日志输出完整路径，并将程序名主体回填到“作业名称”输入框。

## [2026-06-04] - 增加控制器连接断开诊断日志
### 优化
- **涉及文件**: `mainwindow.h`, `mainwindow.cpp`
- **改动说明**:
  - 记录 `connect_robot` 返回的 socket 与连接调用耗时，便于判断是否卡在 SDK 连接超时。
  - 在状态轮询异常时输出 `servo/run/type/pose` 各接口返回值、位姿数组长度、单轮失败数和连续全失败次数。
  - 自动断开前记录本地 socket 与断开判定原因，便于和控制器端连接/断开日志对齐。

## [2026-06-04] - 增加机械臂末端位置姿态显示
### 新增
- **涉及文件**: `mainwindow.h`, `mainwindow.cpp`
- **改动说明**:
  - 在当前 UI 右侧区域增加“末端位置姿态”面板，显示 X、Y、Z、Rx、Ry、Rz 六个末端位姿值。
  - 通过现有 `get_current_position(socket, 1, ...)` 接口轮询笛卡尔末端位姿，并在断开连接或读取失败时显示 `--`。
  - 演示模式下提供固定示例位姿，便于无控制器连接时查看界面效果。
