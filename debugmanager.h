#ifndef DEBUGMANAGER_H
#define DEBUGMANAGER_H

#include <QHash>
#include <QObject>

#include <functional>

namespace gdb {

struct Variable {
    QString name;
    QString type;
    QString value;

    bool isValid() const { return !name.isEmpty() && !value.isEmpty(); }

    static Variable parseMap(const QVariantMap& data);
};

struct Frame {
    int level = -1;
    QString func;
    quint64 addr;
    QHash<QString, QString> params;
    QString file;
    QString fullpath;
    int line;

    bool isValid() const { return level != -1; }

    static Frame parseMap(const QVariantMap& data);
};

struct Breakpoint {
    int number = -1;
    QString type;
    enum Disp_t { keep, del } disp;
    bool enable;
    quint64 addr;
    QString func;
    QString file;
    QString fullname;
    int line;
    QList<QString> threadGroups;
    int times;
    QString originalLocation;

    bool isValid() const { return number != -1; }

    static Breakpoint parseMap(const QVariantMap& data);
};

struct Thread {
    int id;
    QString targetId;
    QString details;
    QString name;
    enum State_t { Unknown, Stopped, Running } state;
    Frame frame;
    int core;

    static Thread parseMap(const QVariantMap& data);
};

}

class DebugManager : public QObject
{
    Q_OBJECT

public:
    enum class ResponseAction_t {
        Permanent,
        Temporal
    };

    using ResponseHandler_t = std::function<void (const QVariant& v)>;

    Q_PROPERTY(QString gdbCommand READ gdbCommand WRITE setGdbCommand)
    Q_PROPERTY(bool remote READ isRemote)
    Q_PROPERTY(bool gdbExecuting READ isGdbExecuting)
    Q_PROPERTY(QStringList gdbArgs READ gdbArgs WRITE setGdbArgs)
#ifdef Q_OS_WIN
    Q_PROPERTY(QString sigintHelperCmd READ sigintHelperCmd WRITE setSigintHelperCmd)
#endif
    static DebugManager *instance();

    QStringList gdbArgs() const;
    QString gdbCommand() const;
    bool isRemote() const;
    bool isGdbExecuting() const;

    QList<gdb::Breakpoint> allBreakpoints() const;
    QList<gdb::Breakpoint> breakpointsForFile(const QString& filePath) const;
    gdb::Breakpoint breakpointById(int id) const;
    gdb::Breakpoint breakpointByFileLine(const QString& path, int line) const;
#ifdef Q_OS_WIN
    QString sigintHelperCmd() const;
#endif
public slots:
    void execute();
    void quit();

    void command(const QString& cmd);
    void commandAndResponse(const QString& cmd,
                            const ResponseHandler_t& handler,
                            ResponseAction_t action = ResponseAction_t::Temporal);

    void breakRemove(int bpid);
    void breakInsert(const QString& path);

    void loadExecutable(const QString& file);
    void launchRemote(const QString& remoteTarget);
    void launchLocal();

    void commandContinue();
    void commandNext();
    void commandStep();
    void commandFinish();
    void commandInterrupt();

    void setGdbCommand(QString gdbCommand);

    void stackListFrames();

    void setGdbArgs(QStringList gdbArgs);

#ifdef Q_OS_WIN
    void setSigintHelperCmd(QString sigintHelperCmd);
#endif

signals:
    void gdbProcessStarted();
    void gdbProcessTerminated();

    void started();
    void terminated();
    void gdbPromt();
    void targetRemoteConnected();
    void gdbError(const QString& msg);

    void asyncRunning(const QString& thid);
    void asyncStopped(const QString& reason, const gdb::Frame& frame, const QString& thid, int core);

    void updateThreads(int currentId, const QList<gdb::Thread>& threads);
    void updateCurrentFrame(const gdb::Frame& frame);
    void updateStackFrame(const QList<gdb::Frame>& stackFrames);
    void updateLocalVariables(const QList<gdb::Variable>& variableList);

    void breakpointInserted(const gdb::Breakpoint& bp);
    void breakpointModified(const gdb::Breakpoint& bp);
    void breakpointRemoved(const gdb::Breakpoint& bp);

    void result(int token, const QString& reason, const QVariant& results); // <token>^...
    void streamConsole(const QString& text);
    void streamTarget(const QString& text);
    void streamGdb(const QString& text);
    void streamDebugInternal(const QString& text);

private slots:
    void processLine(const QString& line);

private:
    explicit DebugManager(QObject *parent = nullptr);
    virtual ~DebugManager();

    struct Priv_t;
    Priv_t *self;
};

Q_DECLARE_METATYPE(gdb::Variable)
Q_DECLARE_METATYPE(gdb::Frame)
Q_DECLARE_METATYPE(gdb::Breakpoint)
Q_DECLARE_METATYPE(gdb::Thread)

#endif // DEBUGMANAGER_H
