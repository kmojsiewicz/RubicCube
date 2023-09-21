#ifndef OGLWIDGET_H
#define OGLWIDGET_H

#include <QWidget>
#include <QOpenGLWidget>
#include <gl/GLU.h>
#include <gl/GL.h>
#include "cube.h"

class OGLWidget : public QGLWidget
{
    Q_OBJECT

public:
    OGLWidget(QWidget *parent = 0);
    ~OGLWidget();

    void setSolvingInterval(int interval);

public slots:
  void on_pushButtonRandom_clicked();
  void on_pushButtonSolve_clicked();
  void solvingTimerTick();

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;

private:

    int     solvingCnt;
    QTimer *solvingTimer;

    QVector2D LMBPressPosition;
    QVector2D RMBPressPosition;
    QVector3D rotationAxis;

    float     mxRotation[16];                                                         // sumowania macierz rotacji
    double    mxLastModel[16];                                                        // macierz modelu

    TCube *cube;
};

#endif // OGLWIDGET_H
