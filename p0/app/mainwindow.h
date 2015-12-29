#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtGui>
#include <QtCore>
#include <QtSql>
#include <QMessageBox>
#include "ui_cd.h"
#include "ui_mainwindow.h"

#define CRYPTOPP_DLL
#include <cryptopp/aes.h>
#include <cryptopp/modes.h>
#include <cryptopp/osrng.h>
#include <cryptopp/hex.h>
#include <cryptopp/filters.h>
#include <iostream>


class Cipher
{
public:

    void setKey(const QString &text)
    {
        using namespace CryptoPP;
        using namespace std;

        SHA256 h;
        string r;
        StringSource(text.toStdString(), true, new HashFilter(h,new StringSink(r)));

        key.setRawData(r.c_str(), r.size());
    }

    QString encrypt(const QString &text)
    {
        using namespace CryptoPP;
        using namespace std;
        CFB_Mode< AES >::Encryption e;
        e.SetKeyWithIV((const byte *)key.constData(), key.size(), (const byte*) key.constData());
        string r;
        StringSource(text.toStdString(), true, new StreamTransformationFilter(e, new StringSink(r)));
        return QString::fromStdString(r);
    }

    QString decrypt(const QString &text)
    {
        using namespace CryptoPP;
        using namespace std;

        CFB_Mode< AES >::Decryption d;
        d.SetKeyWithIV((const byte *)key.constData(), key.size(), (const byte*) key.constData());
        string r;
        StringSource(text.toStdString(), true, new StreamTransformationFilter(d, new StringSink(r)));
        return QString::fromStdString(r);
    }

protected:
    QByteArray key;
};

class cd : public QDialog
{
    Q_OBJECT
public:
    struct data_t
    {
        QString tile;
        QString account;
        QString password;
    };

    explicit cd(QWidget *parent = 0)
        : QDialog(parent), ui(new Ui::cd())
    {
        ui->setupUi(this);
    }

    ~cd()
    {
        delete ui;
    }

    void setData(const data_t &d)
    {
        ui->leAccount->setText( d.account );
        ui->lePassword->setText( d.password);
        ui->leTitle->setText(d.tile);
    }

    data_t data() const
    {
        data_t d;
        d.account = ui->leAccount->text();
        d.password = ui->lePassword->text();
        d.tile = ui->leTitle->text();
        return d;
    }

private slots:

    void on_pbClose_clicked()
    {
        reject();
    }

    void on_pbSave_clicked()
    {
        accept();
    }

private:
    Ui::cd *ui;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit MainWindow(QWidget *parent = 0)
        : QMainWindow(parent)
        , ui(new Ui::MainWindow)
    {
        ui->setupUi(this);
        init();
        refresh();
    }

    ~MainWindow()
    {
        delete ui;
    }

private slots:

    void on_actionOpen_triggered()
    {
        QFileDialog dialog(this);
        dialog.setAcceptMode(QFileDialog::AcceptOpen);
        dialog.setFileMode(QFileDialog::ExistingFile);
        dialog.setViewMode(QFileDialog::List);
        dialog.setNameFilter("Sqlite3 data (*.db3)");

        if ( QDialog::Accepted != dialog.exec())
            return;

        QStringList selected_files = dialog.selectedFiles();

        if (selected_files.isEmpty())
            return;
        close();
        open(selected_files.front());
    }

    void on_actionNew_triggered()
    {
        QFileDialog dialog(this);
        dialog.setAcceptMode(QFileDialog::AcceptSave);
        dialog.setFileMode(QFileDialog::AnyFile);
        dialog.setViewMode(QFileDialog::List);
        dialog.setNameFilter("Sqlite3 data (*.db3)");
        dialog.exec();

        QStringList selected_files = dialog.selectedFiles();

        if (selected_files.isEmpty())
            return;

        close();
        open(selected_files.front());
    }

    void on_actionExit_triggered()
    {
        qApp->quit();
    }

    void on_actionAdd_triggered()
    {
        cd dd;
        int r = dd.exec();

        if (r == QDialog::Accepted)
        {
            cd::data_t d = dd.data();
            QSqlQuery q(db);
            bool prep_ok = q.prepare("INSERT INTO data (title, account, password) "
                          "VALUES (:title, :account, :password)");
            qDebug() << q.lastError();
            q.bindValue(":title", d.tile);
            q.bindValue(":account", d.account);
            q.bindValue(":password", d.password);
            bool s = q.exec();
            QSqlError err =  q.lastError();
            QString err_text = err.text();
            refresh();
        }

    }

    void on_actionDelete_triggered()
    {
        cd dd;
        QList<QListWidgetItem*> items = ui->lwItems->selectedItems();
        if (!items.empty())
        {
            QListWidgetItem *item = items.front();

            if (QMessageBox::Yes == QMessageBox::question(this
                                                          , tr("Delete")
                                                          , tr("Are you sure to delete %1?").arg(item->text())
                                                          , QMessageBox::Yes
                                                          , QMessageBox::No))
            {
                QSqlQuery q(db);
                q.prepare("DELETE FROM data WHERE id = :id");
                q.bindValue(":id", item->data(Qt::UserRole));
                q.exec();
                refresh();
            }
        }
    }

    void on_actionEdit_triggered()
    {
        cd dd;
        QList<QListWidgetItem*> items = ui->lwItems->selectedItems();
        if (!items.empty())
        {
            QListWidgetItem *item = items.front();

            QSqlQuery q(db);
            q.prepare("SELECT title, account, password FROM data WHERE id = :id");
            q.bindValue(":id", item->data(Qt::UserRole));
            q.exec();

            while(q.next())
            {
                cd::data_t d;
                d.account = q.value(1).toString();
                d.password = q.value(2).toString();
                d.tile = q.value(0).toString();
                dd.setData(d);

                if (QDialog::Accepted == dd.exec())
                {
                    d = dd.data();
                    QSqlQuery q(db);
                    q.prepare("UPDATE data SET title = :t, account = :a, password = :p WHERE id = :id");
                    q.bindValue(":id", item->data(Qt::UserRole));
                    q.bindValue(":t", d.tile);
                    q.bindValue(":a", d.account);
                    q.bindValue(":p", d.password);
                    q.exec();
                    refresh();
                }

            }
        }

    }
    
private:

    void init()
    {
        db = QSqlDatabase::addDatabase("QSQLITE");

        open("app.db3");
    }

    void refresh()
    {
        ui->lwItems->clear();
        QSqlQuery q(db);
        q.prepare("SELECT id, title FROM data ORDER BY title");
        qDebug() << q.lastError();
        q.exec();
        while (q.next())
        {
            QString id = q.value(0).toString();
            QString title = q.value(1).toString();

            QListWidgetItem *item = new QListWidgetItem();
            item->setData(Qt::UserRole, id);
            item->setText(title);
            ui->lwItems->addItem(item);
        }

    }

    void open(const QString &path)
    {
        if (db.isOpen())
            return;

        db.setDatabaseName(path);
        db.open();

        if (!db.tables(QSql::Tables).contains("data"))
        {
            QSqlQuery q(db);
            q.prepare("CREATE TABLE data "
                    "( id INTEGER PRIMARY KEY AUTOINCREMENT"
                    ", title TEXT"
                    ", account TEXT"
                    ", password TEXT)");
            qDebug() << q.lastError();
            q.exec();
            qDebug() << q.lastError();
        }

        refresh();
    }

    void close()
    {
        db.close();
    }


    bool is_open;
    QString path;

    Ui::MainWindow *ui;
    QSqlDatabase db;
};



#endif // MAINWINDOW_H
