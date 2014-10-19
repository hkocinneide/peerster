#include "searcher.hh"
#include "chatdialog.hh"
#include "privatedialog.hh"
#include "textentrybox.hh"
#include "fileshare.hh"
#include "peer.hh"

Searcher::Searcher(ChatDialog *dialog, TextEntryBox *searchBox, QListWidget *searchList)
{
  this->dialog = dialog;
  this->searchBox = searchBox;
  this->searchList = searchList;

  searchTimer = new QTimer();
  connect(searchBox, SIGNAL(returnPressed()),
          this, SLOT(searchTermsEntered()));
  connect(searchTimer, SIGNAL(timeout()),
          this, SLOT(searchTimeout()));
}

void Searcher::searchTermsEntered()
{
  QString rawTerms = searchBox->toPlainText();
  qDebug() << "Search terms entered:" << rawTerms;
  searchBox->clear();
  searchList->clear();
  currentSearchTerms = new QStringList(rawTerms.split(" ", QString::SkipEmptyParts));
  for (int i = 0; i < currentSearchTerms->length(); i++)
  {
    qDebug() << "Term:" << (*currentSearchTerms)[i];
  }
  searchTimer->start(1000);
  currentBudget = 1;
  searchTimeout();
}

void Searcher::searchTimeout()
{
  currentBudget *= 2;
  if (currentBudget <= MAXBUDGET)
  {
    for(int i = 0; i < currentSearchTerms->size(); i++)
    {
       QVariantMap msg;
       msg.insert("Origin", QVariant(dialog->originName));
       msg.insert("Search", (*currentSearchTerms)[i]);
       distributeWithBudget(msg, currentBudget);
    }
  }
  else
  {
    searchTimer->stop();
  }
}

void Searcher::receiveSearch(QVariantMap msg)
{
  quint32 budget = msg.value("Budget").toUInt();
  QString org = msg.value("Origin").toString();
  QString searchTerm = msg.value("Search").toString();
  if (budget != 0 && budget != 1)
  {
    QList<SharedFile*> *sflist = dialog->fileshare->sharedFiles;
    QList<SharedFile*> matchlist;
    for (int i = 0; i < sflist->size(); i++)
    {
      SharedFile *sf = (*sflist)[i];
      if (sf->filename.contains(searchTerm))
      {
        qDebug() << "Got a match for" << searchTerm;
        matchlist.append(sf);
      }
    }
    searchResponse(org, searchTerm, matchlist);
  }
}

void Searcher::searchResponse(QString dest, QString term, QList<SharedFile *> sflist)
{
  QVariantMap response;
  if (dialog->routingTable->contains(dest))
  {
    QPair<QHostAddress,quint16> *address = dialog->routingTable->value(dest)->first;
    QHostAddress ip = address->first;
    quint16 port = address->second;

    QVariantList matchNames;
    QVariantList matchIDs;
    for (int i = 0; i < sflist.size(); i++)
    {
      SharedFile *sf = sflist[i];
      matchNames.append(QVariant(sf->filename));
      matchIDs.append(QVariant(*sf->metahash));
    }

    response.insert("Dest", QVariant(dest));
    response.insert("Origin", QVariant(dialog->originName));
    response.insert("HopLimit", QVariant(HOPLIMIT));
    response.insert("SearchReply", QVariant(term));
    response.insert("MatchNames", QVariant(matchNames));
    response.insert("MatchIDs", QVariant(matchIDs));
    dialog->sendVariantMap(ip, port, &response);
  }
  else
  {
    qDebug() << "Weird, we don't have him in our routing table XJFD";
  }
}

void Searcher::addToList(QString name, QByteArray hash)
{
}

void Searcher::searchResult(QVariantMap msg)
{
  QVariantList matchNames = msg.value("MatchNames").toList();
  QVariantList matchIDs = msg.value("MatchIDs").toList();

  int nnames = matchNames.size();
  int nids   = matchIDs.size();
  if (nids == nnames)
  {
    for (int i = 0; i < nids; i++)
    {
      addToList(matchNames[i].toString(), matchIDs[i].toByteArray());
    }
  }
  else
  {
    qDebug() << "OH GOD WHAT IS GOING ON";
  }
}

void Searcher::distributeWithBudget(QVariantMap msg, quint32 budget)
{
  int npeers = dialog->neighbors->count();
  int budgetEach = budget / npeers;
  int budgetRemain = budget % npeers;
  QHashIterator<QString, Peer*> i(*dialog->neighbors);

  while (i.hasNext())
  {
    i.next();
    int b = budgetEach;
    if (budgetRemain > 0)
    {
      b++; budgetRemain--;
    }
    if (b != 0)
    {
      msg.insert("Budget", b);
      qDebug() << "Sending to:" << stringifyHostPort(i.value()->ipAddress, i.value()->udpPortNumber) << "with budget:" << b;
      dialog->sendVariantMap(i.value(), &msg);
    }
  }
}