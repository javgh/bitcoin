#include <string>
#include <vector>

#include <QClipboard>
#include <QInputDialog>
#include <QList>
#include <QListWidgetItem>
#include <QMessageBox>

#include "main.h"
#include "wallet.h"
#include "init.h"
#include "base58.h"

#include "messagepage.h"
#include "ui_messagepage.h"

#include "addressbookpage.h"
#include "guiutil.h"
#include "walletmodel.h"

MessagePage::MessagePage(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MessagePage)
{
    ui->setupUi(this);

#if (QT_VERSION >= 0x040700)
    /* Do not move this to the XML file, Qt before 4.7 will choke on it */
    ui->signFrom->setPlaceholderText(tr("Enter a Bitcoin address (e.g. 1NS17iag9jJgTHD1VXjvLCEnZuQ3rJDE9L)"));
    ui->signature->setPlaceholderText(tr("Click \"Sign Message\" to get signature"));
#endif

    GUIUtil::setupAddressWidget(ui->signFrom, this);
    ui->signature->installEventFilter(this);

    ui->signFrom->setFocus();
}

MessagePage::~MessagePage()
{
    delete ui;
}

void MessagePage::setModel(WalletModel *model)
{
    this->model = model;
}

void MessagePage::setAddress(QString addr)
{
    ui->signFrom->setText(addr);
    ui->message->setFocus();
}

void MessagePage::on_pasteButton_clicked()
{
    setAddress(QApplication::clipboard()->text());
}

void MessagePage::on_addressBookButton_clicked()
{
    AddressBookPage dlg(AddressBookPage::ForSending, AddressBookPage::ReceivingTab, this);
    dlg.setModel(model->getAddressTableModel());
    if(dlg.exec())
    {
        setAddress(dlg.getReturnValue());
    }
}

void MessagePage::on_copyToClipboard_clicked()
{
    QApplication::clipboard()->setText(ui->signature->text());
}

void MessagePage::on_signMessage_clicked()
{
    QString address = ui->signFrom->text();

    CBitcoinAddress addr(address.toStdString());
    if (!addr.IsValid())
    {
        QMessageBox::critical(this, tr("Error signing"), tr("%1 is not a valid address.").arg(address),
                              QMessageBox::Abort, QMessageBox::Abort);
        return;
    }
    CKeyID keyID;
    if (!addr.GetKeyID(keyID))
    {
        QMessageBox::critical(this, tr("Error signing"), tr("%1 does not refer to a key.").arg(address),
                              QMessageBox::Abort, QMessageBox::Abort);
        return;
    }

    WalletModel::UnlockContext ctx(model->requestUnlock());
    if(!ctx.isValid())
    {
        // Unlock wallet was cancelled
        return;
    }

    CKey key;
    if (!pwalletMain->GetKey(keyID, key))
    {
        QMessageBox::critical(this, tr("Error signing"), tr("Private key for %1 is not available.").arg(address),
                              QMessageBox::Abort, QMessageBox::Abort);
        return;
    }

    CDataStream ss(SER_GETHASH, 0);
    ss << strMessageMagic;
    ss << ui->message->document()->toPlainText().toStdString();

    std::vector<unsigned char> vchSig;
    if (!key.SignCompact(Hash(ss.begin(), ss.end()), vchSig))
    {
        QMessageBox::critical(this, tr("Error signing"), tr("Sign failed"),
                              QMessageBox::Abort, QMessageBox::Abort);
    }

    ui->signature->setText(QString::fromStdString(EncodeBase64(&vchSig[0], vchSig.size())));
    ui->signature->setFont(GUIUtil::bitcoinAddressFont());
}

void MessagePage::on_clearButton_clicked()
{
    ui->signFrom->clear();
    ui->message->clear();
    ui->signature->clear();

    ui->signFrom->setFocus();
}

bool MessagePage::eventFilter(QObject *object, QEvent *event)
{
    if(object == ui->signature && (event->type() == QEvent::MouseButtonPress ||
                                   event->type() == QEvent::FocusIn))
    {
        ui->signature->selectAll();
        return true;
    }
    return QDialog::eventFilter(object, event);
}
