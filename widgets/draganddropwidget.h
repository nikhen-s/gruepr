#ifndef DRAGANDDROPWIDGET_H
#define DRAGANDDROPWIDGET_H

#include <QWidget>

class DragAndDropWidget : public QWidget
{
    Q_OBJECT

public:
    DragAndDropWidget(QWidget *parent = nullptr);
    void dragEnterEvent(QDragEnterEvent *event = nullptr);
    void dropEvent(QDropEvent *event = nullptr);

signals:
    void itemDropped(const QString &filePathString);
};


#endif // DRAGANDDROPWIDGET_H
