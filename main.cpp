#include <unistd.h>
#include <sys/types.h>

#include <QtGui>

#ifdef Q_WS_MAC
// #include <QMacStyle>
#endif

#include "main_window.h"

static char EMessage[1000];

int main(int argc, char *argv[])
{ Error_Buffer = EMessage;

  QApplication app(argc, argv);

  DotWindow::openDialog = new OpenDialog(NULL);

  DotWindow::openFile();

  return app.exec();
}
