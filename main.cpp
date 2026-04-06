#include <unistd.h>
#include <sys/types.h>

#include <QtGui>

#ifdef Q_WS_MAC
// #include <QMacStyle>
#endif

#include "main_window.h"

static char EMessage[ERROR_BUFFER_LEN];

int main(int argc, char *argv[])
{ char *alnPath = NULL;

  Error_Buffer = EMessage;

  //  Save file argument before QApplication modifies argc/argv
  if (argc > 1)
    alnPath = argv[1];

  QApplication app(argc, argv);

  DotWindow::openDialog = new OpenDialog(NULL);

  if (alnPath != NULL)
    DotWindow::openPath(alnPath);
  else
    DotWindow::openFile();

  return app.exec();
}
