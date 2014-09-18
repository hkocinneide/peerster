#include "textentrybox.hh"

TextEntryBox::TextEntryBox(QWidget *parent) : QTextEdit(parent)
{
}

void TextEntryBox::keyPressEvent(QKeyEvent *e)
{
  if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter)
  {
    emit returnPressed();
  }
  else
  {
    QTextEdit::keyPressEvent(e);
  }
}

