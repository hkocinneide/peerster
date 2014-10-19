#include "qinclude.hh"

#define MAXBUDGET 100
#define STARTBUDGET 2

class ChatDialog;
class TextEntryBox;
class SharedFile;

class Searcher : public QObject
{
  Q_OBJECT

public:
  Searcher(ChatDialog *dialog, TextEntryBox *searchBox, QListWidget *searchList);
  void distributeWithBudget(QVariantMap, quint32);
  void receiveSearch(QVariantMap);
  void searchResult(QVariantMap);

public slots:
  void searchTermsEntered();
  void searchTimeout();

private:
  QTimer *searchTimer;
  quint32 currentBudget;
  ChatDialog *dialog;
  TextEntryBox *searchBox;
  QListWidget *searchList;
  QStringList *currentSearchTerms;

  void searchResponse(QString, QString, QList<SharedFile*>);
  void addToList(QString, QByteArray);
};
