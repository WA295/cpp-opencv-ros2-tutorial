# 提交项目到 GitHub 的完整步骤（小白版）//AI编写

> 适用环境：本机（Ubuntu 24.04 + 已登录 GitHub 账号 WA295）。
> 本机特殊情况：终端访问 GitHub 需要走代理 `127.0.0.1:7890`，下面命令已包含。

---

## 0. 先搞懂三件事

| 概念 | 大白话 |
| --- | --- |
| 本地仓库 | 你电脑上的项目文件夹（比如 `~/code_learn`） |
| 提交 commit | 给当前文件状态"拍一张存档照"（存在本地） |
| 推送 push | 把本地存档上传到 GitHub（远程仓库） |

**一句话流程：改文件 → 存档（commit）→ 上传（push）。**

---

## 1. 一次性准备（已经做过了，新电脑才需要）

```bash
# 1. 装工具（没有 gh 时）
sudo apt install git gh

# 2. 登录 GitHub（会给你一个设备码，去 https://github.com/login/device 输入）
gh auth login --hostname github.com --git-protocol https --web

# 3. 配置身份（改成你的名字和邮箱）
git config --global user.name "WA295"
git config --global user.email "326531788+WA295@users.noreply.github.com"

# 4. 终端走代理（本机网络需要，每次开新终端推送前执行一次）
export HTTPS_PROXY=http://127.0.0.1:7890 HTTP_PROXY=http://127.0.0.1:7890
```

> ✅ 你现在这台电脑：第 1~4 步都已完成，**直接用第 2 节的三条命令就行**。

---

## 2. 已有仓库，更新并推送（最常用）

**场景：** 改了 `code_learn` 里的文档，想同步到 GitHub。

```bash
cd ~/code_learn

export HTTPS_PROXY=http://127.0.0.1:7890 HTTP_PROXY=http://127.0.0.1:7890

git add -A
git commit -m "说清楚这次改了什么"
git push
```

**完了。** 去仓库页面刷新就能看到更新。

**辅助命令：**

| 想干嘛 | 命令 |
| --- | --- |
| 看有哪些改动 | `git status` |
| 看提交历史 | `git log --oneline` |
| 看改了哪些内容 | `git diff` |
| 撤销某文件的改动 | `git checkout -- 文件名` |
| 撤销最后一次 commit（没 push 时） | `git reset --soft HEAD~1` |

---

## 3. 从零提交一个新项目

假设你要提交一个新项目 `~/my_project`：

### 第 1 步：进入项目文件夹并初始化

```bash
cd ~/my_project
git init -b main
```

### 第 2 步：写一个 .gitignore（排除不该上传的文件）

```bash
cat > .gitignore <<'EOF'
build/
install/
log/
*.o
__pycache__/
.vscode/
EOF
```

> 规则：`build/` 表示忽略 build 目录，`*.o` 表示忽略所有 .o 文件。

### 第 3 步：写 README 和 LICENSE（可选但推荐）

- `README.md`：项目说明（GitHub 主页会自动显示）
- `LICENSE`：开源许可证（MIT / Apache-2.0 等）

### 第 4 步：第一次存档

```bash
git add -A
git commit -m "初始提交"
```

### 第 5 步：在 GitHub 上建仓库并推送（一条命令）

```bash
export HTTPS_PROXY=http://127.0.0.1:7890 HTTP_PROXY=http://127.0.0.1:7890

gh repo create 仓库名 --public --source=. --remote=origin --push
```

- `仓库名` 换成你想要的（如 `my-project`）
- `--public` 是公开；想私有改成 `--private`
- 完成后会打印仓库网址，浏览器打开即可

### 第 6 步：以后更新，回到第 2 节的三条命令

---

## 4. 不想用命令行建仓库？用网页也行

1. 打开 github.com → 右上角 **+** → **New repository**
2. 填仓库名、选 Public/Private → **Create repository**（不要勾 README，保持空仓库）
3. 回到终端：

```bash
cd ~/my_project
git init -b main
git add -A
git commit -m "初始提交"
git branch -M main
git remote add origin https://github.com/你的用户名/仓库名.git
export HTTPS_PROXY=http://127.0.0.1:7890 HTTP_PROXY=http://127.0.0.1:7890
git push -u origin main
```

> 推荐用第 3 节的 `gh repo create` 方式，步骤最少。

---

## 5. 常用命令速查表

| 命令 | 作用 |
| --- | --- |
| `git status` | 看当前有哪些改动 |
| `git add -A` | 把所有改动加入"待存档" |
| `git add 文件名` | 只加入某个文件 |
| `git commit -m "说明"` | 存档 |
| `git push` | 上传到 GitHub |
| `git pull` | 从 GitHub 拉取更新（多人协作/多台电脑时） |
| `git log --oneline` | 看提交历史 |
| `git diff` | 看具体改了什么 |
| `git clone 网址` | 把远程仓库下载到本地 |
| `git remote -v` | 看远程仓库地址 |

---

## 6. 常见问题

| 现象 | 原因 | 解决 |
| --- | --- | --- |
| `git push` 卡住/超时 | 代理没设 | 先执行 `export HTTPS_PROXY=http://127.0.0.1:7890 HTTP_PROXY=http://127.0.0.1:7890` |
| `fatal: not a git directory` | 没在仓库里 | `cd` 到项目文件夹（有 `.git` 的地方） |
| `Please tell me who you are` | 没配身份 | 执行第 1 节第 3 步的 `git config` |
| 提示要输入 GitHub 用户名密码 | 凭证失效 | 重新 `gh auth login`，再 `gh auth setup-git` |
| 提交了不该提交的文件 | 忘了 .gitignore | 加进 .gitignore，然后 `git rm -r --cached 路径` |
| commit 说明写错了 | 想改 | 没 push：`git commit --amend -m "新说明"` |
| 改乱了想回退 | 想恢复 | `git checkout -- 文件名`（放弃改动）或 `git log` 找版本回退 |

---

## 7. 提交规范小建议

1. **每次只做一件事**：一个 commit 对应一个改动（"修复文档笔误"、"新增练手项目"）。
2. **说明写清楚**：`git commit -m "README: 添加徽章和英文简介"`，别写 `"更新"`。
3. **小步提交**：做完一个功能就 commit 一次，别攒一个月。
4. **别提交大文件**：视频、数据集、编译产物都不要进 git（用 .gitignore）。
5. **push 前先 `git status`**：确认要提交的东西都对。

---

> 一句话总结：
> **`git add -A` → `git commit -m "说明"` → `git push`**
> 就这三条，记住它们你就掌握了日常使用。

---

> 上一篇：[环境与工具速查](01-环境与工具速查.md)
> 下一篇：[C++ 基础语法与头文件](../01-C++/00-C++基础语法与头文件.md)
