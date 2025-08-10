#include "FrameGraphTooltip.h"
#include "FrameGraphViewer.h"
#include "widgets/CustomPaintWidget.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QStylePainter>

FrameGraphTooltip::FrameGraphTooltip(FrameGraphViewer *parent, CustomPaintWidget *thumbnail,
                                     ICaptureContext &ctx)
    : QFrame(parent), m_Ctx(ctx)
{
  int margin = style()->pixelMetric(QStyle::PM_ToolTipLabelFrameWidth, NULL, this);
  int opacity = style()->styleHint(QStyle::SH_ToolTipLabel_Opacity, NULL, this);

  m_FrameGraphViewer = parent;

  setWindowFlags(Qt::ToolTip | Qt::WindowStaysOnTopHint);
  setAttribute(Qt::WA_TransparentForMouseEvents);
  setForegroundRole(QPalette::ToolTipText);
  setBackgroundRole(QPalette::ToolTipBase);
  setFrameStyle(QFrame::NoFrame);
  setWindowOpacity(opacity / 255.0);
  setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

  QHBoxLayout *hbox = new QHBoxLayout;
  QVBoxLayout *vbox = new QVBoxLayout;
  hbox->setSpacing(0);
  hbox->setContentsMargins(0, 0, 0, 0);
  vbox->setSpacing(2);
  vbox->setContentsMargins(6, 3, 6, 3);

  label = new QLabel(this);
  label->setAlignment(Qt::AlignLeft);

  title = new QLabel(this);
  title->setAlignment(Qt::AlignLeft);

  setLayout(vbox);
  vbox->addWidget(title);
  vbox->addLayout(hbox);

  hbox->addWidget(thumbnail);
  hbox->addStretch();

  vbox->addWidget(label);
}



void FrameGraphTooltip::hideTip()
{
  hide();
}

QSize FrameGraphTooltip::configureTip(ResourceId resourceId, QString text)
{
  ResourceId id = ResourceId();

  id = m_FrameGraphViewer->updateThumbnail(resourceId);

  if(id != ResourceId())
  {
    title->setText(m_Ctx.GetResourceName(id));
    title->show();
  }
  else
  {
    title->hide();
  }
  label->setText(text);
  label->setVisible(!text.isEmpty());
  layout()->update();
  layout()->activate();
  return minimumSizeHint();
}

void FrameGraphTooltip::showTip(QPoint pos)
{
  move(pos);
  resize(minimumSize());
  show();
}

bool FrameGraphTooltip::hasThumbnail(ResourceId resourceId)
{
  return m_FrameGraphViewer->hasThumbnail(resourceId);
}

void FrameGraphTooltip::update()
{
  CustomPaintWidget *thumbnail = this->findChild<CustomPaintWidget *>();
  if(thumbnail)
    thumbnail->update();
}

void FrameGraphTooltip::paintEvent(QPaintEvent *ev)
{
  QStylePainter p(this);
  QStyleOptionFrame opt;
  opt.init(this);
  p.drawPrimitive(QStyle::PE_PanelTipLabel, opt);
  p.end();

  QWidget::paintEvent(ev);
}

void FrameGraphTooltip::resizeEvent(QResizeEvent *e)
{
  QStyleHintReturnMask frameMask;
  QStyleOption option;
  option.init(this);
  if(style()->styleHint(QStyle::SH_ToolTip_Mask, &option, this, &frameMask))
    setMask(frameMask.region);

  QWidget::resizeEvent(e);
}
