#include "dialogstartdebug.h"
#include "ui_dialogstartdebug.h"

#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTextStream>
#include <QProcessEnvironment>

static QStringList findPatternInPath(const QRegularExpression& re) {
    QSet<QString> fileList;
    QLatin1Char PATH_SEPARATOR
#ifdef Q_OS_WIN
    {';'}
#else
    {':'}
#endif
    ;
    auto paths = QProcessEnvironment::systemEnvironment().value("PATH").split(PATH_SEPARATOR);
    for (const auto& e: paths) {
        for (const auto& f: QDir{e}.entryInfoList()) {
            auto name = f.fileName();
            auto filePath = f.absoluteFilePath();
            if (re.match(name).hasMatch())
                fileList += filePath;
        }
    }
    return fileList.toList();
}

static bool processObject(QComboBox *b, const QJsonObject& j)
{
    auto name = j.value("name").toString();
    b->addItem(name, j.toVariantMap());
    if (j.value("default").toBool(false))
        return true;
    return false;
}

DialogStartDebug::DialogStartDebug(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DialogStartDebug)
{
    m_needWriteInitScript = true;
    ui->setupUi(this);

    auto templateList =
        QDir{":/gdbinit"}.entryInfoList({"*.json"}) +
        QDir{QApplication::applicationDirPath() + "/../share/gdbfront/"}.entryInfoList({"gdbinit*.json"}) +
        QDir{QApplication::applicationDirPath() + "/gdbfront/"}.entryInfoList({"gdbinit*.json"});
    int defaultIdx = 0;
    for (const auto& e: templateList) {
        QFile f{e.absoluteFilePath()};
        if (f.open(QFile::ReadOnly)) {
            auto doc = QJsonDocument::fromJson(f.readAll());
            if (doc.isObject()) {
                if (processObject(ui->comboGdbInitTemplates, doc.object()))
                    defaultIdx = ui->comboGdbInitTemplates->count() - 1;
            } else if (doc.isArray()) {
                for (const auto& g: doc.array()) {
                    if (processObject(ui->comboGdbInitTemplates, g.toObject()))
                        defaultIdx = ui->comboGdbInitTemplates->count() - 1;
                }
            }
        }
    }
    auto detectGdbFor = [this](int idx) {
        auto j = ui->comboGdbInitTemplates->itemData(idx).toMap();
        auto pattern = j.value("preferredGdb").toString();
        auto preferredGdb = QRegularExpression{pattern, QRegularExpression::MultilineOption};
        ui->editorInitScript->setPlainText(j.value("commands").toStringList().join('\n'));
        ui->editorGdbExecFile->setCurrentIndex(-1);
        for (int i=0; i<ui->editorGdbExecFile->count(); i++) {
            QFileInfo info{ui->editorGdbExecFile->itemText(i)};
            auto name = info.baseName();
            if (preferredGdb.match(name).hasMatch()) {
                ui->editorGdbExecFile->setCurrentIndex(i);
                break;
            }
        }
    };
    connect(ui->comboGdbInitTemplates, QOverload<int>::of(&QComboBox::currentIndexChanged), detectGdbFor);
    ui->comboGdbInitTemplates->setCurrentIndex(defaultIdx);

    ui->editorGdbExecFile->clear();
    ui->editorGdbExecFile->addItems(findPatternInPath(QRegularExpression{R"(^([\w_\-]+\-)?gdb(\.exe)?$)"}));
    detectGdbFor(defaultIdx);

    connect(ui->buttonChoseExecutable, &QToolButton::clicked, [this]() {
        auto name = QFileDialog::getOpenFileName(this, tr("Select file"));
        if (!name.isEmpty()) {
            ui->editorExecFile->setText(name);
            loadInitScript();
        }
    });
    connect(ui->buttonChoseGdbExecutable, &QToolButton::clicked, [this]() {
        auto name = QFileDialog::getOpenFileName(this, tr("Select file"), {}, tr("gdb executable (*gdb*);;All files (*)"));
        if (!name.isEmpty()) {
            int idx = ui->editorGdbExecFile->findText(name);
            if (!idx) {
                ui->editorGdbExecFile->insertItem(0, name);
                idx = 0;
            }
            ui->editorGdbExecFile->setCurrentIndex(idx);
            loadInitScript();
        }
    });
    connect(ui->editorInitScript, &QPlainTextEdit::modificationChanged, [this](bool isModified) {
        if (isModified)
            m_needWriteInitScript = true;
    });
    connect(ui->editorExecFile, &QLineEdit::editingFinished, [this]() {
        if (!ui->editorInitScript->isWindowModified())
            loadInitScript();
    });
}

DialogStartDebug::~DialogStartDebug()
{
    delete ui;
}

QString DialogStartDebug::executableFile() const
{
    return ui->editorExecFile->text();
}

QString DialogStartDebug::initScriptName() const
{
    return m_initScriptName;
}

QString DialogStartDebug::initScript() const
{
    return ui->editorInitScript->toPlainText();
}

QString DialogStartDebug::gdbExecutable() const
{
    return ui->editorGdbExecFile->currentText();
}

void DialogStartDebug::loadInitScript(const QString &path)
{
    QFileInfo initScript{path};
    if (path.isEmpty()) {
        if (ui->editorExecFile->text().isEmpty())
            return;
        initScript.setFile(QFileInfo{ui->editorExecFile->text()}.absoluteDir().filePath(".gdbinit"));
    }
    if (initScript.exists()) {
        m_initScriptName = initScript.absoluteFilePath();
        QFile f{m_initScriptName};
        if (f.open(QFile::ReadOnly)) {
            QTextStream ss(&f);
            ui->editorInitScript->setPlainText(ss.readAll());
            ui->editorInitScript->setWindowModified(false);
        }
    }
}
