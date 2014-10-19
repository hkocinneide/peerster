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
  fileNameList = new QHash<QString, QPair<QString,QByteArray*>*>();
  connect(searchBox, SIGNAL(returnPressed()),
          this, SLOT(searchTermsEntered()));
  connect(searchTimer, SIGNAL(timeout()),
          this, SLOT(searchTimeout()));
  connect(searchList, SIGNAL(itemActivated(QListWidgetItem *)),
          this, SLOT(fileActivated(QListWidgetItem *)));
}

void Searcher::fileActivated(QListWidgetItem *item)
{
  if (fileNameList->contains(item->text()))
  {
    dialog->fileshare->waitingOnBlockList = true;
    dialog->fileshare->waitingOnFileName = item->text();
    QPair<QString,QByteArray*> *entry = fileNameList->value(item->text());
    QString node = entry->first;
    QByteArray *id = entry->second;
    dialog->fileshare->requestBlock(node, *id);
  }
}

void Searcher::searchTermsEntered()
{
  QString rawTerms = searchBox->toPlainText();
  qDebug() << "Search terms entered:" << rawTerms;
  searchBox->clear();
  searchList->clear();
  delete fileNameList;
  fileNameList = new QHash<QString,QPair<QString,QByteArray*>*>();
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
    QByteArray matchIDs;
    for (int i = 0; i < sflist.size(); i++)
    {
      SharedFile *sf = sflist[i];
      matchNames.append(QVariant(sf->filename));
      matchIDs.append(*sf->metahash);
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

void Searcher::addToList(QString name, QByteArray hash, QString org)
{
  if (!fileNameList->contains(name))
  {
    QPair<QString, QByteArray*> *entry = new QPair<QString, QByteArray*>(org, new QByteArray(hash));
    fileNameList->insert(name, entry);
    new QListWidgetItem(name, searchList);
  }
}

void Searcher::searchResult(QVariantMap msg)
{
  QVariantList matchNames = msg.value("MatchNames").toList();
  QByteArray matchIDs = msg.value("MatchIDs").toByteArray();
  QList<QByteArray> matchIDList;
  int size = matchIDs.size();

  if (size % 20 == 0)
  {
    int pos = 0;
    while (pos < size)
    {
      QByteArray a = matchIDs.mid(pos, 20);
      matchIDList.append(a);
      pos += 20;
    }
  }
  else
  {
    qDebug() << "Oh no this isn't right this isn't right this isn't right";
    return;
  }


  int nnames = matchNames.size();
  int nids   = matchIDList.size();
  if (nids == nnames)
  {
    for (int i = 0; i < nids; i++)
    {
      addToList(matchNames[i].toString(), matchIDList[i], msg.value("Origin").toString());
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
