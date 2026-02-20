#ifndef NV_PLATFORM_SHORTCUTS_HELP_H
#define NV_PLATFORM_SHORTCUTS_HELP_H

#include <QString>
#include <QMessageBox>
#include <QWidget>

namespace nv {

inline QString shortcuts_help_text() {
#ifdef Q_OS_MACOS
    const QString primary = "Cmd";
    const QString secondary = "Ctrl";
#else
    const QString primary = "Ctrl";
    const QString secondary = "Ctrl";
#endif

    QString text;
    text += "Global\n";
    text += QString("  %1+L    Focus search\n").arg(secondary);
    text += QString("  %1+R    Rename selected note\n").arg(secondary);
    text += QString("  %1+S    Save current note\n").arg(primary);
    text += QString("  %1+Q    Quit application\n").arg(primary);
#ifndef Q_OS_MACOS
    text += QString("  %1+,    Open Notes Directory settings\n").arg(primary);
#endif
    text += "\n";

    text += "Edit\n";
    text += QString("  %1+Z    Undo\n").arg(primary);
    text += QString("  %1+Y    Redo\n").arg(primary);
    text += QString("  %1+X    Cut\n").arg(primary);
    text += QString("  %1+C    Copy\n").arg(primary);
    text += QString("  %1+V    Paste\n").arg(primary);
    text += QString("  %1+A    Select all\n").arg(primary);
    text += "\n";

    text += "Search Field\n";
    text += "  Enter    Select first match or create a new note\n";
    text += "  Esc      Clear search\n";
    text += "  Tab      Focus note list\n";
    text += "\n";

    text += "Note List\n";
    text += "  Up/Down  Move selection\n";
    text += "  Enter    Focus editor/checklist\n";
    text += "  Tab      Focus editor/checklist\n";
    text += "\n";

    text += "Editor\n";
    text += "  Tab      Focus search field\n";
    text += "\n";

    text += "Checklist Editor\n";
    text += "  Up/Down  Move between checklist items\n";
    text += "  Enter    Add new checklist item\n";
    text += "  Tab      Focus search field\n";
    text += QString("  %1+T    Toggle current checkbox\n").arg(secondary);
    text += QString("  %1+D    Delete current checklist item\n").arg(secondary);

    return text;
}

inline void show_shortcuts_popup(QWidget* parent) {
    QMessageBox box(parent);
    box.setWindowTitle("Keyboard Shortcuts");
    box.setIcon(QMessageBox::Information);
    box.setText(shortcuts_help_text());
    box.exec();
}

} // namespace nv

#endif // NV_PLATFORM_SHORTCUTS_HELP_H