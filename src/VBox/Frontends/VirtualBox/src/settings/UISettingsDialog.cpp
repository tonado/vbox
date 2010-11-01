/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UISettingsDialog class implementation
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Global includes */
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>

/* Local includes */
#include "UISettingsDialog.h"
#include "VBoxWarningPane.h"
#include "VBoxGlobal.h"
#include "VBoxProblemReporter.h"
#include "QIWidgetValidator.h"
#include "VBoxSettingsSelector.h"
#include "UISettingsPage.h"
#include "UIToolBar.h"
#include "UIIconPool.h"
#ifdef Q_WS_MAC
# include "VBoxUtils.h"
# if MAC_LEOPARD_STYLE
#  define VBOX_GUI_WITH_TOOLBAR_SETTINGS
# endif /* MAC_LEOPARD_STYLE */
#endif /* Q_WS_MAC */

/* Settings Dialog Constructor: */
UISettingsDialog::UISettingsDialog(QWidget *pParent /* = 0 */)
    /* Parent class: */
    : QIWithRetranslateUI<QIMainDialog>(pParent)
    /* Protected variables: */
    , m_pSelector(0)
    , m_pStack(0)
    /* Common variables: */
    , m_fPolished(false)
    /* Error/Warning stuff: */
    , m_fValid(true)
    , m_fSilent(true)
    , m_pWarningPane(new VBoxWarningPane(this))
    /* Whats-this stuff: */
    , m_pWhatsThisTimer(new QTimer(this))
    , m_pWhatsThisCandidate(0)
{
    /* Apply UI decorations: */
    Ui::UISettingsDialog::setupUi(this);

#ifdef Q_WS_MAC
    /* No status bar on the mac: */
    setSizeGripEnabled(false);
    setStatusBar(0);
#endif /* Q_WS_MAC */

    /* Page-title font is derived from the system font: */
    QFont pageTitleFont = font();
    pageTitleFont.setBold(true);
    pageTitleFont.setPointSize(pageTitleFont.pointSize() + 2);
    m_pLbTitle->setFont(pageTitleFont);

    /* Get main grid layout: */
    QGridLayout *pMainLayout = static_cast<QGridLayout*>(centralWidget()->layout());
#ifdef VBOX_GUI_WITH_TOOLBAR_SETTINGS
    /* No page-title with tool-bar: */
    m_pLbTitle->hide();
    /* No whats-this with tool-bar: */
    m_pLbWhatsThis->hide();
    /* Create modern tool-bar selector: */
    m_pSelector = new VBoxSettingsToolBarSelector(this);
    static_cast<UIToolBar*>(m_pSelector->widget())->setMacToolbar();
    addToolBar(qobject_cast<QToolBar*>(m_pSelector->widget()));
    /* No title in this mode, we change the title of the window: */
    pMainLayout->setColumnMinimumWidth(0, 0);
    pMainLayout->setHorizontalSpacing(0);
#else
    /* Create classical tree-view selector: */
    m_pSelector = new VBoxSettingsTreeViewSelector(this);
    pMainLayout->addWidget(m_pSelector->widget(), 0, 0, 3, 1);
    m_pSelector->widget()->setFocus();
    pMainLayout->setSpacing(10);
#endif /* VBOX_GUI_WITH_TOOLBAR_SETTINGS */

    /* Creating stack of pages: */
    m_pStack = new QStackedWidget(m_pWtStackHandler);
    QVBoxLayout *pStackLayout = new QVBoxLayout(m_pWtStackHandler);
    pStackLayout->setContentsMargins(0, 0, 0, 0);
    pStackLayout->addWidget(m_pStack);

    /* Setup error & warning stuff: */
    m_pButtonBox->addExtraWidget(m_pWarningPane);
    m_errorIcon = UIIconPool::defaultIcon(UIIconPool::MessageBoxCriticalIcon, this).pixmap(16, 16);
    m_warningIcon = UIIconPool::defaultIcon(UIIconPool::MessageBoxWarningIcon, this).pixmap(16, 16);

    /* Setup whatsthis stuff: */
    qApp->installEventFilter(this);
    m_pWhatsThisTimer->setSingleShot(true);
    connect(m_pWhatsThisTimer, SIGNAL(timeout()), this, SLOT(sltUpdateWhatsThis()));
    m_pLbWhatsThis->setAutoFillBackground(true);
    QPalette whatsThisPalette = m_pLbWhatsThis->palette();
    whatsThisPalette.setBrush(QPalette::Window, whatsThisPalette.brush(QPalette::Midlight));
    m_pLbWhatsThis->setPalette(whatsThisPalette);
    m_pLbWhatsThis->setFixedHeight(m_pLbWhatsThis->frameWidth() * 2 +
                                   m_pLbWhatsThis->margin() * 2 +
                                   m_pLbWhatsThis->fontMetrics().lineSpacing() * 4);

    /* Set the default button: */
    m_pButtonBox->button(QDialogButtonBox::Ok)->setDefault(true);

    /* Setup connections: */
    connect(m_pSelector, SIGNAL(categoryChanged(int)), this, SLOT(sltCategoryChanged(int)));
    connect(m_pButtonBox, SIGNAL(helpRequested()), &vboxProblem(), SLOT(showHelpHelpDialog()));

    /* Translate UI: */
    retranslateUi();
}

UISettingsDialog::~UISettingsDialog()
{
    /* Delete selector early! */
    delete m_pSelector;
}

void UISettingsDialog::sltRevalidate(QIWidgetValidator *pValidator)
{
    /* Get related settings page: */
    UISettingsPage *pSettingsPage = qobject_cast<UISettingsPage*>(pValidator->widget());
    AssertMsg(pSettingsPage, ("Validator should corresponds a page!\n"));

    /* Prepare empty warning & title: */
    QString strWarning;
    QString strTitle = m_pSelector->itemTextByPage(pSettingsPage);

    /* Revalidate the page: */
    bool fValid = pSettingsPage->revalidate(strWarning, strTitle);

    /* If revalidation is fully passed - recorrelate the pages: */
    if (fValid && strWarning.isEmpty())
        fValid = recorrelate(pSettingsPage, strWarning);

    /* Compose a message: */
    strWarning = strWarning.isEmpty() ? QString() :
                 tr("On the <b>%1</b> page, %2").arg(strTitle, strWarning);
    pValidator->setLastWarning(strWarning);
    fValid ? setWarning(strWarning) : setError(strWarning);

    /* Remember validation status: */
    pValidator->setOtherValid(fValid);
}

void UISettingsDialog::sltCategoryChanged(int cId)
{
    QWidget *pRootPage = m_pSelector->rootPage(cId);
#ifdef Q_WS_MAC
    QSize cs = size();
    /* First make all fully resizeable: */
    setMinimumSize(QSize(minimumWidth(), 0));
    setMaximumSize(QSize(minimumWidth(), QWIDGETSIZE_MAX));
    for (int i = 0; i < m_pStack->count(); ++i)
        m_pStack->widget(i)->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Ignored);
    int a = m_pStack->indexOf(pRootPage);
    if (a < m_sizeList.count())
    {
        QSize ss = m_sizeList.at(a);
        m_pStack->widget(a)->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        /* Switch to the new page first if we are shrinking: */
        if (cs.height() > ss.height())
            m_pStack->setCurrentIndex(m_pStack->indexOf(pRootPage));
        /* Do the animation: */
        ::darwinWindowAnimateResize(this, QRect (x(), y(), ss.width(), ss.height()));
        /* Switch to the new page last if we are zooming: */
        if (cs.height() <= ss.height())
            m_pStack->setCurrentIndex(m_pStack->indexOf(pRootPage));
        /* Make the widget fixed size: */
        setFixedSize(ss);
    }
    ::darwinSetShowsResizeIndicator(this, false);
#else
    m_pLbTitle->setText(m_pSelector->itemText(cId));
    m_pStack->setCurrentIndex(m_pStack->indexOf(pRootPage));
#endif
#ifdef VBOX_GUI_WITH_TOOLBAR_SETTINGS
    setWindowTitle(title());
#endif /* VBOX_GUI_WITH_TOOLBAR_SETTINGS */
}

void UISettingsDialog::retranslateUi()
{
    /* Translate generated stuff: */
    Ui::UISettingsDialog::retranslateUi(this);

    /* Translate error/warning stuff: */
    m_strErrorHint = tr("Invalid settings detected");
    m_strWarningHint = tr("Non-optimal settings detected");
    if (!m_fValid)
        m_pWarningPane->setWarningText(m_strErrorHint);
    else if (!m_fSilent)
        m_pWarningPane->setWarningText(m_strWarningHint);

    /* Get the list of validators: */
    QList<QIWidgetValidator*> validatorsList = findChildren<QIWidgetValidator*>();
    /* Retranslate all validators: */
    for (int i = 0; i < validatorsList.size(); ++i)
    {
        QIWidgetValidator *pValidator = validatorsList[i];
        pValidator->setCaption(m_pSelector->itemTextByPage(qobject_cast<UISettingsPage*>(pValidator->widget())));
    }
    /* Revalidate all pages to retranslate the warning messages also: */
    for (int i = 0; i < validatorsList.size(); ++i)
    {
        QIWidgetValidator *pValidator = validatorsList[i];
        if (!pValidator->isValid())
            sltRevalidate(pValidator);
    }
}

QString UISettingsDialog::titleExtension() const
{
#ifdef VBOX_GUI_WITH_TOOLBAR_SETTINGS
    return m_pSelector->itemText(m_pSelector->currentId());
#else
    return tr("Settings");
#endif
}

void UISettingsDialog::setError(const QString &strError)
{
    m_strErrorString = strError.isEmpty() ? QString() :
                       QString("<font color=red>%1</font>").arg(strError);

    /* Do not touching QILabel until dialog is polished
     * otherwise it can change its size to undefined: */
    if (m_fPolished)
    {
        if (!m_strErrorString.isEmpty())
            m_pLbWhatsThis->setText(m_strErrorString);
        else
            sltUpdateWhatsThis(true /* got focus? */);
    }
}

void UISettingsDialog::setWarning(const QString &strWarning)
{
    m_strWarningString = strWarning.isEmpty() ? QString() :
                         QString("<font color=#ff5400>%1</font>").arg(strWarning);

    /* Do not touching QILabel until dialog is polished
     * otherwise it can change its size to undefined: */
    if (m_fPolished)
    {
        if (!m_strWarningString.isEmpty())
            m_pLbWhatsThis->setText(m_strWarningString);
        else
            sltUpdateWhatsThis(true /* got focus? */);
    }
}

void UISettingsDialog::addItem(const QString &strBigIcon,
                               const QString &strBigIconDisabled,
                               const QString &strSmallIcon,
                               const QString &strSmallIconDisabled,
                               int cId,
                               const QString &strLink,
                               UISettingsPage *pSettingsPage /* = 0 */,
                               int iParentId /* = -1 */)
{
    QWidget *pPage = m_pSelector->addItem(strBigIcon, strBigIconDisabled,
                                          strSmallIcon, strSmallIconDisabled,
                                          cId, strLink, pSettingsPage, iParentId);
    if (pPage)
        m_pStack->addWidget(pPage);
    if (pSettingsPage)
        assignValidator(pSettingsPage);
}

bool UISettingsDialog::recorrelate(QWidget * /* pPage */, QString & /* strWarning */)
{
    return true;
}

void UISettingsDialog::sltHandleValidityChanged(const QIWidgetValidator * /* pValidator */)
{
    /* Get validators list: */
    QList<QIWidgetValidator*> validatorsList(findChildren<QIWidgetValidator*>());

    /* Detect ERROR presence: */
    {
        setError(QString());
        QString strError;
        bool fNewValid = true;
        for (int i = 0; i < validatorsList.size(); ++i)
        {
            QIWidgetValidator *pValidator = validatorsList[i];
            fNewValid = pValidator->isValid();
            if (!fNewValid)
            {
                strError = pValidator->warningText();
                if (strError.isNull())
                    strError = pValidator->lastWarning();
                break;
            }
        }

        /* Try to set the generic error message when invalid
         * but no specific message is provided: */
        if (m_strErrorString.isNull() && !strError.isNull())
            setError(strError);

        m_fValid = fNewValid;
        m_pWarningPane->setWarningPixmap(m_errorIcon);
        m_pWarningPane->setWarningText(m_strErrorHint);
#ifdef Q_WS_MAC
        m_pWarningPane->setToolTip(m_strErrorString);
#endif /* Q_WS_MAC */
        m_pWarningPane->setVisible(!m_fValid);
        m_pButtonBox->button(QDialogButtonBox::Ok)->setEnabled(m_fValid);

        if (!m_fValid)
            return;
    }

    /* Detect WARNING presence: */
    {
        setWarning(QString());
        QString strWarning;
        bool fNewSilent = true;
        for (int i = 0; i < validatorsList.size(); ++i)
        {
            QIWidgetValidator *pValidator = validatorsList[i];
            if (!pValidator->warningText().isNull() || !pValidator->lastWarning().isNull())
            {
                fNewSilent = false;
                strWarning = pValidator->warningText();
                if (strWarning.isNull())
                    strWarning = pValidator->lastWarning();
                break;
            }
        }

        /* Try to set the generic error message when invalid
         * but no specific message is provided: */
        if (m_strWarningString.isNull() && !strWarning.isNull())
            setWarning(strWarning);

        m_fSilent = fNewSilent;
        m_pWarningPane->setWarningPixmap(m_warningIcon);
        m_pWarningPane->setWarningText(m_strWarningHint);
#ifdef Q_WS_MAC
        m_pWarningPane->setToolTip(m_strWarningString);
#endif /* Q_WS_MAC */
        m_pWarningPane->setVisible(!m_fSilent);
    }
}

void UISettingsDialog::sltUpdateWhatsThis(bool fGotFocus /* = false */)
{
    QString strWhatsThisText;
    QWidget *pWhatsThisWidget = 0;

    /* If focus had NOT changed: */
    if (!fGotFocus)
    {
        /* We will use the recommended candidate: */
        if (m_pWhatsThisCandidate && m_pWhatsThisCandidate != this)
            pWhatsThisWidget = m_pWhatsThisCandidate;
    }
    /* If focus had changed: */
    else
    {
        /* We will use the focused widget instead: */
        pWhatsThisWidget = QApplication::focusWidget();
    }

    /* If the given widget lacks the whats-this text, look at its parent: */
    while (pWhatsThisWidget && pWhatsThisWidget != this)
    {
        strWhatsThisText = pWhatsThisWidget->whatsThis();
        if (!strWhatsThisText.isEmpty())
            break;
        pWhatsThisWidget = pWhatsThisWidget->parentWidget();
    }

#ifndef Q_WS_MAC
    if (strWhatsThisText.isEmpty() && !m_strErrorString.isEmpty())
        strWhatsThisText = m_strErrorString;
    else if (strWhatsThisText.isEmpty() && !m_strWarningString.isEmpty())
        strWhatsThisText = m_strWarningString;
    if (strWhatsThisText.isEmpty())
        strWhatsThisText = whatsThis();
    m_pLbWhatsThis->setText(strWhatsThisText);
#else
    if (pWhatsThisWidget && !strWhatsThisText.isEmpty())
        pWhatsThisWidget->setToolTip(QString("<qt>%1</qt>").arg(strWhatsThisText));
#endif
}

bool UISettingsDialog::eventFilter(QObject *pObject, QEvent *pEvent)
{
    /* Ignore objects which are NOT widgets: */
    if (!pObject->isWidgetType())
        return QIMainDialog::eventFilter(pObject, pEvent);

    /* Ignore widgets which window is NOT settings dialog: */
    QWidget *pWidget = static_cast<QWidget*>(pObject);
    if (pWidget->window() != this)
        return QIMainDialog::eventFilter(pObject, pEvent);

    /* Process different event-types: */
    switch (pEvent->type())
    {
        /* Process enter/leave events to remember whats-this candidates: */
        case QEvent::Enter:
        case QEvent::Leave:
        {
            if (pEvent->type() == QEvent::Enter)
                m_pWhatsThisCandidate = pWidget;
            else
                m_pWhatsThisCandidate = 0;

            m_pWhatsThisTimer->start(100);
            break;
        }
        /* Process focus-in event to update whats-this pane: */
        case QEvent::FocusIn:
        {
            sltUpdateWhatsThis(true /* got focus? */);
            break;
        }
        default:
            break;
    }

    /* Base-class processing: */
    return QIMainDialog::eventFilter(pObject, pEvent);
}

void UISettingsDialog::showEvent(QShowEvent *pEvent)
{
    /* Base-class processing: */
    QIMainDialog::showEvent(pEvent);

    /* One may think that QWidget::polish() is the right place to do things
     * below, but apparently, by the time when QWidget::polish() is called,
     * the widget style & layout are not fully done, at least the minimum
     * size hint is not properly calculated. Since this is sometimes necessary,
     * we provide our own "polish" implementation. */
    if (m_fPolished)
        return;

    m_fPolished = true;

    int iMinWidth = m_pSelector->minWidth();
#ifdef Q_WS_MAC
    /* Remove all title bar buttons (Buggy Qt): */
    ::darwinSetHidesAllTitleButtons(this);

    /* Set all size policies to ignored: */
    for (int i = 0; i < m_pStack->count(); ++i)
        m_pStack->widget(i)->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Ignored);
    /* Activate every single page to get the optimal size: */
    for (int i = m_pStack->count() - 1; i >= 0; --i)
    {
        m_pStack->widget(i)->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        m_pStack->setCurrentIndex(i);
        layout()->activate();
        QSize s = minimumSize();
        if (iMinWidth > s.width())
            s.setWidth(iMinWidth);
        m_sizeList.insert(0, s);
        m_pStack->widget(i)->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Ignored);
    }

    sltCategoryChanged(m_pSelector->currentId());
#else /* Q_WS_MAC */
    /* Resize to the minimum possible size: */
    QSize s = minimumSize();
    if (iMinWidth > s.width())
        s.setWidth(iMinWidth);
    resize(s);
#endif /* Q_WS_MAC */
}

void UISettingsDialog::assignValidator(UISettingsPage *pPage)
{
    QIWidgetValidator *pValidator = new QIWidgetValidator(m_pSelector->itemTextByPage(pPage), pPage, this);
    connect(pValidator, SIGNAL(validityChanged(const QIWidgetValidator*)), this, SLOT(sltHandleValidityChanged(const QIWidgetValidator*)));
    connect(pValidator, SIGNAL(isValidRequested(QIWidgetValidator*)), this, SLOT(sltRevalidate(QIWidgetValidator*)));
    pPage->setValidator(pValidator);
    pPage->setOrderAfter(m_pSelector->widget());
}

