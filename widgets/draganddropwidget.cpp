#include "draganddropwidget.h"
#include <QDropEvent>
#include <QMimeData>
#include "gruepr_globals.h"

DragAndDropWidget::DragAndDropWidget(QWidget *parent) : QWidget(parent) {
    setAcceptDrops(true);
    setStyleSheet("QWidget {background-color: " OPENWATERHEX "; color: white; font-family:'DM Sans'; font-size: 12pt; "
                  "border-style: solid; border-width: 2px; border-radius: 5px; border-color: white; padding: 10px;}"
                  "QLabel { border: none; padding: 0px;}");
}


void DragAndDropWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()){
        event->acceptProposedAction();
    }
}

void DragAndDropWidget::dropEvent(QDropEvent *event)
{

    QList<QUrl> urls = event->mimeData()->urls();
    QString filePathString;

    foreach (const QUrl &url, urls){
        filePathString += url.toLocalFile() + ";";
    }
    emit itemDropped(filePathString);
    qDebug() << "Dropped url" << filePathString;
}

