import io

# ============ A. highlighters: 新增专用 YamlHighlighter ============
h = io.open('src/widgets/highlighters.h', encoding='utf-8').read()
old = '''// Shell/kubectl 命令高亮（K8s 命令生成器）'''
new = '''// YAML 专用高亮（K8s 模板页）
class YamlHighlighter : public ThemedHighlighter {
    Q_OBJECT
public:
    using ThemedHighlighter::ThemedHighlighter;
protected:
    void rebuildRules() override;
};

// Shell/kubectl 命令高亮（K8s 命令生成器）'''
assert old in h
h = h.replace(old, new)
io.open('src/widgets/highlighters.h', 'w', encoding='utf-8', newline='\n').write(h)
print('h ok')

c = io.open('src/widgets/highlighters.cpp', encoding='utf-8').read()
anchor = '''// ---------------- ShellHighlighter ----------------'''
yaml_impl = r'''// ---------------- YamlHighlighter ----------------
void YamlHighlighter::rebuildRules() {
    m_patterns.clear();
    m_formats.clear();
    const Palette p = pal();

    auto add = [this](const QString& pattern, const QColor& color, bool bold = false) {
        QTextCharFormat fmt;
        fmt.setForeground(color);
        if (bold) fmt.setFontWeight(QFont::Bold);
        m_patterns.append(QRegularExpression(pattern));
        m_formats.append(fmt);
    };

    add(QStringLiteral("#[^\\n]*"), p.dim);                                            // 注释
    add(QStringLiteral("'[^']*'|\"[^\"]*\""), p.str);                                  // 引号串
    add(QStringLiteral("(?<=[{,])[A-Za-z_][\\w.-]*(?=\\s*:)"), p.key);                 // flow 键 {cpu: ..}
    add(QStringLiteral("^[ \\t]*-?[ \\t]*[^\\s:#{}\\[\\]][^:\\n]{0,60}(?=:(\\s|$))"), p.key, true);  // 键（含 "- name:"）
    add(QStringLiteral("\\b(?:true|false|null|yes|no|on|off)\\b"), p.boolean);         // 布尔
    add(QStringLiteral("\\b\\d+(?:\\.\\d+)?\\b"), p.num);                              // 数字
    add(QStringLiteral("^[ \\t]*-[ \\t]+"), p.accent);                                 // 列表符
    add(QStringLiteral("(?<=^|\\s)---(?=\\s|$)"), p.dim);                              // 文档分隔
    add(QStringLiteral("[&*][A-Za-z_]\\w*"), p.accent);                                // anchor/alias
    rebuild();
}

// ---------------- ShellHighlighter ----------------'''
assert anchor in c
c = c.replace(anchor, yaml_impl, 1)
io.open('src/widgets/highlighters.cpp', 'w', encoding='utf-8', newline='\n').write(c)
print('cpp ok')

# ============ B. k8stoolspages：Yaml 页换高亮器；Cmd 页删执行 ============
s = io.open('src/tools/k8stoolspages.cpp', encoding='utf-8').read()

# B1. YamlPage 高亮器
old = '''        m_out->setPlaceholderText(QStringLiteral("选择模板后在此生成，可直接编辑…"));
        new CodeHighlighter(m_out->document());'''
new = '''        m_out->setPlaceholderText(QStringLiteral("选择模板后在此生成，可直接编辑…"));
        new YamlHighlighter(m_out->document());'''
assert old in s, 'b1'
s = s.replace(old, new)

# B2. ctor 删执行按钮（cmdBtns 只留复制）
old = '''        m_runBtn = ui::button(QStringLiteral("▶ 执行"), "primary");
        connect(m_runBtn, &QPushButton::clicked, this, [this] { runCommands(); });
        auto* cmdBtns = new QWidget;
        auto* cbLay = new QHBoxLayout(cmdBtns);
        cbLay->setContentsMargins(0, 0, 0, 0);
        cbLay->setSpacing(6);
        cbLay->addWidget(m_copyBtn);
        cbLay->addWidget(m_runBtn);
        pageLay->addWidget(ui::card(QStringLiteral("命令 · 参数变化实时更新"), m_out, cmdBtns));'''
new = '''        pageLay->addWidget(ui::card(QStringLiteral("命令 · 参数变化实时更新（复制到终端执行）"), m_out, m_copyBtn));'''
assert old in s, 'b2'
s = s.replace(old, new)

# B3. 删 tokenify
old = '''// 把整条命令拆成参数列表：支持 'xxx' 与 "xxx" 包裹含空格/JSON 的参数
static QStringList tokenify(const QString& cmd) {
    QStringList out;
    const int sp = static_cast<int>(cmd.indexOf(QLatin1Char(' ')));
    if (sp < 0) return {cmd};
    out << cmd.left(sp);
    QString cur;
    bool inS = false, inD = false, has = false;
    for (const QChar& ch : cmd.mid(sp + 1)) {
        if (ch == QLatin1Char('\\'') && !inD) { inS = !inS; has = true; continue; }
        if (ch == QLatin1Char('"') && !inS) { inD = !inD; has = true; continue; }
        if (ch.isSpace() && !inS && !inD) {
            if (has) { out << cur; cur.clear(); has = false; }
            continue;
        }
        cur += ch;
        has = true;
    }
    if (has) out << cur;
    return out;
}

'''
assert old in s, 'b3'
s = s.replace(old, '')

# B4. 删 runCommands/runNext
old = '''    // 执行：输出直接写入命令区（控制台模式）；执行中按钮变「■ 停止」
    void runCommands() {
        if (m_executing) {
            if (m_proc && m_proc->state() != QProcess::NotRunning) m_proc->kill();
            return;
        }
        if (!m_kubectlOk) {
            m_out->setPlainText(QStringLiteral("# ✗ 未检测到 kubectl，请先点击「一键安装 kubectl」或自行安装\\n# （修改任意参数可恢复命令显示）"));
            return;
        }
        const QStringList lines = m_lastCmd.split(QLatin1Char('\\n'), Qt::SkipEmptyParts);
        if (lines.isEmpty()) return;
        m_out->clear();
        m_queue = lines;
        m_executing = true;
        m_runBtn->setText(QStringLiteral("■ 停止"));
        runNext();
    }
    void runNext() {
        if (m_queue.isEmpty()) {
            m_executing = false;
            m_runBtn->setText(QStringLiteral("▶ 执行"));
            m_out->appendPlainText(QStringLiteral("—— 执行完成（修改任意参数恢复命令显示）——"));
            return;
        }
        const QString cmd = m_queue.takeFirst();
        m_out->appendPlainText(QStringLiteral("$ %1").arg(cmd));
        delete m_proc;
        m_proc = new QProcess(this);
        m_proc->setProcessChannelMode(QProcess::MergedChannels);
        connect(m_proc, &QProcess::readyReadStandardOutput, this, [this] {
            while (m_proc && m_proc->canReadLine())
                m_out->appendPlainText(QString::fromUtf8(m_proc->readLine()).trimmed());
        });
        connect(m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
                [this](int code, QProcess::ExitStatus) {
            if (code != 0)
                m_out->appendPlainText(QStringLiteral("✗ 退出码 %1").arg(code));
            runNext();
        });
        const QStringList tokens = tokenify(cmd);
        m_proc->start(tokens.value(0), tokens.mid(1));
    }

'''
assert old in s, 'b4'
s = s.replace(old, '')

# B5. scheduleRegen 恢复简单版
old = '''    void scheduleRegen() {
        if (m_executing) return;   // 执行期间保持输出视图，结束后任意参数变化即恢复命令
        m_regenTimer.start();
    }'''
new = '''    void scheduleRegen() { m_regenTimer.start(); }'''
assert old in s, 'b5'
s = s.replace(old, new)

# B6. 成员清理
old = '''    QPushButton* m_runBtn = nullptr;
    QProcess* m_proc = nullptr;
    QNetworkAccessManager m_nam;
    QStringList m_queue;
    bool m_kubectlOk = false;
    bool m_executing = false;'''
new = '''    QNetworkAccessManager m_nam;
    bool m_kubectlOk = false;'''
assert old in s, 'b6'
s = s.replace(old, new)

io.open('src/tools/k8stoolspages.cpp', 'w', encoding='utf-8', newline='\n').write(s)
print('k8s ok')

# ============ C. 版本 ============
for path, old, new in [
    ('CMakeLists.txt', 'project(lingwtools VERSION 1.4.0', 'project(lingwtools VERSION 1.4.1'),
    ('src/main.cpp', 'setApplicationVersion(QStringLiteral("1.4.0"))', 'setApplicationVersion(QStringLiteral("1.4.1"))'),
    ('scripts/installer.iss', '#define MyAppVersion "1.4.0"', '#define MyAppVersion "1.4.1"'),
    ('README.md', 'lingwtools-setup-1.4.0.exe', 'lingwtools-setup-1.4.1.exe'),
]:
    t = io.open(path, encoding='utf-8').read()
    assert old in t, 'NOT FOUND in ' + path
    io.open(path, 'w', encoding='utf-8', newline='\n').write(t.replace(old, new))
    print('bumped', path)
print('ALL OK')
