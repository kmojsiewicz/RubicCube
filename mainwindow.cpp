#include "mainwindow.h"
#include "oglwidget.h"
#include "ui_mainwindow.h"

//---------------------------------------------------------------------------
MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    QGridLayout *gridLayout = new QGridLayout(ui->groupBox);

    Widget = new OGLWidget(ui->groupBox);
    gridLayout->addWidget(Widget);
    Widget->show();
    Widget->setFocusPolicy(Qt::StrongFocus);
    Widget->setFocus();

    connect(ui->pushButtonRandom, SIGNAL(clicked()), Widget, SLOT(on_pushButtonRandom_clicked()));
    connect(ui->pushButtonSolve, SIGNAL(clicked()), Widget, SLOT(on_pushButtonSolve_clicked()));
}

//---------------------------------------------------------------------------
MainWindow::~MainWindow()
{
    delete ui;
}
//---------------------------------------------------------------------------
