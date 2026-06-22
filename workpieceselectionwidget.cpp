#include "workpieceselectionwidget.h"

#include <QApplication>
#include <QBoxLayout>
#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QStyle>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>

#include <cmath>
#include <utility>

namespace {

QStringList imageFilters()
{
    return {
        QStringLiteral("*.png"),
        QStringLiteral("*.jpg"),
        QStringLiteral("*.jpeg"),
        QStringLiteral("*.bmp"),
        QStringLiteral("*.webp")
    };
}

constexpr int kThumbnailGridWidth = 170;
constexpr int kThumbnailGridHeight = 142;
constexpr int kThumbnailColumnCount = 2;
constexpr int kSwipeThreshold = 70;

const QString kTabActiveStyle = QStringLiteral(
    "QPushButton {"
    "background-color: #45475a;"
    "color: #89b4fa;"
    "border: 1px solid #89b4fa;"
    "border-radius: 4px;"
    "padding: 5px 14px;"
    "font-size: 24px;"
    "font-weight: bold;"
    "min-height: 52px;"
    "}");

const QString kTabInactiveStyle = QStringLiteral(
    "QPushButton {"
    "background-color: #313244;"
    "color: #a6adc8;"
    "border: 1px solid #45475a;"
    "border-radius: 4px;"
    "padding: 5px 14px;"
    "font-size: 24px;"
    "min-height: 52px;"
    "}"
    "QPushButton:hover {"
    "background-color: #45475a;"
    "border-color: #89b4fa;"
    "}");

const QString kFavOnStyle = QStringLiteral(
    "QPushButton {"
    "background-color: #4e3e1e;"
    "color: #f9e2af;"
    "border: 1px solid #f9e2af;"
    "border-radius: 5px;"
    "padding: 6px 16px;"
    "font-size: 24px;"
    "min-height: 56px;"
    "}"
    "QPushButton:hover {"
    "background-color: #6e5e2e;"
    "}");

const QString kFavOffStyle = QStringLiteral(
    "QPushButton {"
    "background-color: #313244;"
    "color: #cdd6f4;"
    "border: 1px solid #45475a;"
    "border-radius: 5px;"
    "padding: 6px 16px;"
    "font-size: 24px;"
    "min-height: 56px;"
    "}"
    "QPushButton:hover {"
    "background-color: #45475a;"
    "border-color: #f9e2af;"
    "}");

const QString kSelectStyle = QStringLiteral(
    "QPushButton {"
    "background-color: #1e4620;"
    "color: #a6e3a1;"
    "border: 1px solid #a6e3a1;"
    "border-radius: 5px;"
    "padding: 6px 16px;"
    "font-size: 24px;"
    "min-height: 56px;"
    "}"
    "QPushButton:hover {"
    "background-color: #2d6930;"
    "border-color: #c9f7c5;"
    "color: #d7ffd4;"
    "}"
    "QPushButton:pressed {"
    "background-color: #3b7f3f;"
    "}"
    "QPushButton:disabled {"
    "background-color: #1e1e2e;"
    "color: #585b70;"
    "border-color: #313244;"
    "}");

} // namespace

WorkpieceSelectionWidget::WorkpieceSelectionWidget(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
    loadModelMapping();
    loadFavorites();
    loadImages();
}

QWidget* WorkpieceSelectionWidget::selectionActionBar() const
{
    return m_selectionActionBar;
}

void WorkpieceSelectionWidget::addPage(const QString& title, QWidget *page)
{
    if (!m_pageStack || !page) {
        return;
    }

    const int pageIndex = m_pageStack->addWidget(page);
    QPushButton *pageButton = new QPushButton(title, this);
    pageButton->setStyleSheet(kTabInactiveStyle);
    pageButton->setFixedHeight(64);
    pageButton->setFixedWidth(220);
    pageButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_extraPageButtons.append(pageButton);

    QBoxLayout *rootLayout = qobject_cast<QBoxLayout*>(layout());
    QBoxLayout *switchRow = rootLayout && rootLayout->count() > 0
        ? qobject_cast<QBoxLayout*>(rootLayout->itemAt(0)->layout())
        : nullptr;
    if (switchRow) {
        switchRow->insertWidget(qMax(0, switchRow->count() - 1), pageButton);
    }

    connect(pageButton, &QPushButton::clicked, this, [this, pageIndex]() {
        if (m_pageStack) {
            m_pageStack->setCurrentIndex(pageIndex);
        }
        updatePageSwitch();
    });
    updatePageSwitch();
}

bool WorkpieceSelectionWidget::eventFilter(QObject *watched, QEvent *event)
{
    const bool isThumbnailViewport = m_thumbnailList && watched == m_thumbnailList->viewport();
    const bool isPreviewSurface = watched == m_previewArea || watched == m_previewLabel;
    if (!isThumbnailViewport && !isPreviewSurface) {
        return QWidget::eventFilter(watched, event);
    }

    switch (event->type()) {
    case QEvent::Resize:
        if (isPreviewSurface) {
            schedulePreviewRefresh();
        }
        break;
    case QEvent::MouseButtonPress: {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            m_pressPos = mouseEvent->pos();
            m_draggingThumbList = false;
            m_draggingSwipe = false;
        }
        break;
    }
    case QEvent::MouseMove: {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->buttons() & Qt::LeftButton) {
            const QPoint delta = mouseEvent->pos() - m_pressPos;
            if (!m_draggingThumbList && !m_draggingSwipe &&
                (std::abs(delta.x()) >= QApplication::startDragDistance() ||
                 std::abs(delta.y()) >= QApplication::startDragDistance())) {
                if (std::abs(delta.x()) > std::abs(delta.y())) {
                    m_draggingSwipe = true;
                } else if (isThumbnailViewport) {
                    m_draggingThumbList = true;
                    m_thumbnailList->setCursor(Qt::ClosedHandCursor);
                }
            }
            if (m_draggingThumbList) {
                m_thumbnailList->verticalScrollBar()->setValue(
                    m_thumbnailList->verticalScrollBar()->value() - delta.y());
                m_pressPos = mouseEvent->pos();
                return true;
            }
        }
        break;
    }
    case QEvent::MouseButtonRelease:
        if (m_draggingSwipe) {
            const int deltaX = static_cast<QMouseEvent *>(event)->pos().x() - m_pressPos.x();
            if (std::abs(deltaX) >= kSwipeThreshold) {
                selectAdjacentWorkpiece(deltaX < 0 ? 1 : -1);
            }
            m_draggingSwipe = false;
            return true;
        }
        if (m_draggingThumbList) {
            m_draggingThumbList = false;
            if (m_thumbnailList) {
                m_thumbnailList->unsetCursor();
            }
            return true;
        }
        break;
    default:
        break;
    }

    return QWidget::eventFilter(watched, event);
}

void WorkpieceSelectionWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    schedulePreviewRefresh();
}

void WorkpieceSelectionWidget::onThumbnailChanged(int row)
{
    if (row < 0 || row >= m_imagePaths.size()) {
        m_currentPixmap = QPixmap();
        m_currentId.clear();
        m_previewNameLabel->setText(QStringLiteral("No workpiece selected"));
        m_previewDescLabel->setText(QString());
        updateFavoriteButton();
        updatePreview();
        return;
    }

    const QString baseName = QFileInfo(m_imagePaths.at(row)).completeBaseName();
    m_currentPixmap.load(m_imagePaths.at(row));
    m_currentId = baseName;
    m_previewNameLabel->setText(baseName);

    const QString desc = m_idToDescription.value(baseName);
    m_previewDescLabel->setText(desc.isEmpty() ? QString() : desc);

    updateFavoriteButton();
    updatePreview();
}

void WorkpieceSelectionWidget::onSearch()
{
    const QString id = m_searchEdit->text().trimmed();
    if (id.isEmpty()) {
        return;
    }

    // Look through all image paths for a matching ID
    for (int i = 0; i < m_allImagePaths.size(); ++i) {
        if (QFileInfo(m_allImagePaths.at(i)).completeBaseName() == id) {
            // If we are in favorites mode and the item is not a favorite,
            // switch back to "all" so the user can see it
            if (m_showingFavorites && !m_favoriteIds.contains(id)) {
                onShowAllTab();
            }
            const QString categoryCode = m_idToCategoryCode.value(id, id.left(2));
            const int categoryIndex = m_categoryCombo ? m_categoryCombo->findData(categoryCode) : -1;
            if (categoryIndex >= 0 && categoryIndex != m_categoryCombo->currentIndex()) {
                m_categoryCombo->setCurrentIndex(categoryIndex);
            }
            selectWorkpieceById(id);
            return;
        }
    }

    QMessageBox msgBox(this);
    msgBox.setWindowTitle(QStringLiteral("Search Result"));
    msgBox.setText(QStringLiteral("Workpiece ID not found: %1").arg(id));
    msgBox.setIcon(QMessageBox::Information);
    msgBox.exec();
}

void WorkpieceSelectionWidget::onToggleFavorite()
{
    if (m_currentId.isEmpty()) {
        return;
    }

    if (m_favoriteIds.contains(m_currentId)) {
        m_favoriteIds.remove(m_currentId);
    } else {
        m_favoriteIds.insert(m_currentId);
    }

    saveFavorites();
    updateFavoriteButton();

    // If showing favorites and the item was just unfavorited, refresh the list
    if (m_showingFavorites) {
        const QString rememberedId = m_currentId;
        refreshThumbnailList();
        // Try to re-select the same item if it's still visible
        selectWorkpieceById(rememberedId);
    }
}

void WorkpieceSelectionWidget::onShowAllTab()
{
    if (!m_showingFavorites) {
        return;
    }

    m_showingFavorites = false;
    m_tabAllBtn->setStyleSheet(kTabActiveStyle);
    m_tabFavBtn->setStyleSheet(kTabInactiveStyle);

    const QString rememberedId = m_currentId;
    refreshThumbnailList();
    selectWorkpieceById(rememberedId);
}

void WorkpieceSelectionWidget::onShowFavoritesTab()
{
    if (m_showingFavorites) {
        return;
    }

    m_showingFavorites = true;
    m_tabFavBtn->setStyleSheet(kTabActiveStyle);
    m_tabAllBtn->setStyleSheet(kTabInactiveStyle);

    const QString rememberedId = m_currentId;
    refreshThumbnailList();
    selectWorkpieceById(rememberedId);
}

void WorkpieceSelectionWidget::onCategoryChanged(int index)
{
    if (!m_categoryCombo || index < 0 || index >= m_categoryCodes.size()) {
        return;
    }

    const QString categoryCode = m_categoryCodes.at(index);
    if (categoryCode == m_currentCategoryCode) {
        return;
    }

    m_currentCategoryCode = categoryCode;
    refreshThumbnailList();
    if (m_thumbnailList->count() > 0) {
        m_thumbnailList->setCurrentRow(0);
    }
}

void WorkpieceSelectionWidget::onShowSelectionPage()
{
    if (m_pageStack) {
        m_pageStack->setCurrentIndex(0);
    }
    updatePageSwitch();
    layoutPreviewArea();
    updatePreview();
}

void WorkpieceSelectionWidget::onShowThumbnailPage()
{
    if (m_pageStack) {
        m_pageStack->setCurrentIndex(1);
    }
    updatePageSwitch();
}

void WorkpieceSelectionWidget::buildUi()
{
    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(4);

    QHBoxLayout *pageSwitchRow = new QHBoxLayout();
    pageSwitchRow->setContentsMargins(0, 0, 0, 0);
    pageSwitchRow->setSpacing(8);

    m_pageSelectionBtn = new QPushButton(QStringLiteral("工件选择"), this);
    m_pageSelectionBtn->setStyleSheet(kTabActiveStyle);
    m_pageSelectionBtn->setFixedHeight(64);
    m_pageSelectionBtn->setFixedWidth(220);
    m_pageSelectionBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    pageSwitchRow->addWidget(m_pageSelectionBtn);

    m_pageThumbnailBtn = new QPushButton(QStringLiteral("工件缩略图"), this);
    m_pageThumbnailBtn->setStyleSheet(kTabInactiveStyle);
    m_pageThumbnailBtn->setFixedHeight(64);
    m_pageThumbnailBtn->setFixedWidth(220);
    m_pageThumbnailBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    pageSwitchRow->addWidget(m_pageThumbnailBtn);
    pageSwitchRow->addStretch(1);

    root->addLayout(pageSwitchRow);

    m_pageStack = new QStackedWidget(this);
    m_pageStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    root->addWidget(m_pageStack, 1);

    // 鈹€鈹€ Left: Preview frame 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€
    QFrame *previewFrame = new QFrame(m_pageStack);
    previewFrame->setObjectName(QStringLiteral("workpiecePreviewFrame"));
    previewFrame->setFrameShape(QFrame::StyledPanel);
    previewFrame->setStyleSheet(QStringLiteral(
        "#workpiecePreviewFrame {"
        "background-color: #1e1e2e;"
        "border: 1px solid #45475a;"
        "border-radius: 6px;"
        "}"
        "#workpiecePreviewFrame QLabel {"
        "color: #cdd6f4;"
        "}"));

    QVBoxLayout *previewLayout = new QVBoxLayout(previewFrame);
    previewLayout->setContentsMargins(14, 14, 14, 14);
    previewLayout->setSpacing(8);

    m_previewNameLabel = new QLabel(QStringLiteral("工件预览"), previewFrame);
    m_previewNameLabel->setProperty("styleClass", "header");
    m_previewNameLabel->setStyleSheet(QStringLiteral("color: #cdd6f4; font-size: 14px; font-weight: bold;"));
    previewLayout->addWidget(m_previewNameLabel);

    m_previewDescLabel = new QLabel(previewFrame);
    m_previewDescLabel->setWordWrap(true);
    m_previewDescLabel->setStyleSheet(QStringLiteral("color: #a6adc8; font-size: 12px;"));
    previewLayout->addWidget(m_previewDescLabel);

    m_previewArea = new QWidget(previewFrame);
    m_previewArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_previewArea->installEventFilter(this);

    QVBoxLayout *previewAreaLayout = new QVBoxLayout(m_previewArea);
    previewAreaLayout->setContentsMargins(0, 0, 0, 0);
    previewAreaLayout->addStretch();

    m_previewLabel = new QLabel(m_previewArea);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_previewLabel->installEventFilter(this);
    m_previewLabel->setStyleSheet(QStringLiteral(
        "QLabel {"
        "background-color: #11111b;"
        "border: 1px solid #313244;"
        "border-radius: 8px;"
        "}"));
    previewAreaLayout->addWidget(m_previewLabel, 0, Qt::AlignCenter);
    previewAreaLayout->addStretch();

    previewLayout->addWidget(m_previewArea, 1);

    QFrame *selectionActionBar = new QFrame(this);
    selectionActionBar->setObjectName(QStringLiteral("workpieceSelectionActionBar"));
    selectionActionBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    selectionActionBar->setStyleSheet(QStringLiteral(
        "#workpieceSelectionActionBar {"
        "background-color: #11111b;"
        "border: 1px solid #313244;"
        "border-radius: 8px;"
        "}"));
    QVBoxLayout *selectionLayout = new QVBoxLayout(selectionActionBar);
    selectionLayout->setContentsMargins(8, 8, 8, 8);
    selectionLayout->setSpacing(8);

    QHBoxLayout *categoryRow = new QHBoxLayout();
    categoryRow->setSpacing(6);
    QLabel *categoryLabel = new QLabel(QStringLiteral("品类"), selectionActionBar);
    categoryLabel->setProperty("styleClass", "subtle");
    categoryLabel->setMinimumHeight(56);
    categoryLabel->setStyleSheet(QStringLiteral("font-size: 24px;"));
    categoryRow->addWidget(categoryLabel);
    m_categoryCombo = new QComboBox(selectionActionBar);
    m_categoryCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_categoryCombo->setStyleSheet(QStringLiteral(
        "QComboBox { font-size: 24px; min-height: 56px; }"
        "QComboBox QAbstractItemView { font-size: 24px; }"));
    categoryRow->addWidget(m_categoryCombo, 1);
    selectionLayout->addLayout(categoryRow);

    QHBoxLayout *searchRow = new QHBoxLayout();
    searchRow->setSpacing(6);

    m_searchEdit = new QLineEdit(selectionActionBar);
    m_searchEdit->setPlaceholderText(QStringLiteral("输入工件 ID，例如 100001"));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setStyleSheet(QStringLiteral("QLineEdit { font-size: 24px; min-height: 56px; }"));
    searchRow->addWidget(m_searchEdit, 1);

    m_searchBtn = new QPushButton(QStringLiteral("搜索"), selectionActionBar);
    m_searchBtn->setProperty("styleClass", "primary");
    m_searchBtn->setFixedWidth(m_searchBtn->sizeHint().width() * 2);
    searchRow->addWidget(m_searchBtn);

    selectionLayout->addLayout(searchRow);

    QHBoxLayout *buttonRow = new QHBoxLayout();
    buttonRow->setContentsMargins(8, 8, 8, 8);
    buttonRow->setSpacing(6);

    m_favoriteBtn = new QPushButton(QStringLiteral("收藏"), selectionActionBar);
    m_favoriteBtn->setStyleSheet(kFavOffStyle);
    buttonRow->addWidget(m_favoriteBtn, 1);

    m_selectBtn = new QPushButton(QStringLiteral("选择"), selectionActionBar);
    m_selectBtn->setProperty("styleClass", "success");
    m_selectBtn->setStyleSheet(kSelectStyle);
    m_selectBtn->setEnabled(false);
    buttonRow->addWidget(m_selectBtn, 4);

    selectionLayout->addLayout(buttonRow);

    m_selectionActionBar = selectionActionBar;

    m_hintLabel = new QLabel(
        QStringLiteral("预览区按 4:3 比例适配图片，按住左键左右滑动可切换工件。"),
        previewFrame);
    m_hintLabel->setWordWrap(true);
    m_hintLabel->setProperty("styleClass", "subtle");
    previewLayout->addWidget(m_hintLabel);

    m_pageStack->addWidget(previewFrame);

    // 鈹€鈹€ Right: Thumbnail frame 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€
    QFrame *thumbFrame = new QFrame(m_pageStack);
    thumbFrame->setObjectName(QStringLiteral("workpieceThumbFrame"));
    thumbFrame->setFrameShape(QFrame::StyledPanel);
    thumbFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    thumbFrame->setStyleSheet(QStringLiteral(
        "#workpieceThumbFrame {"
        "background-color: #1e1e2e;"
        "border: 1px solid #45475a;"
        "border-radius: 6px;"
        "}"
        "#workpieceThumbFrame QLabel {"
        "color: #cdd6f4;"
        "}"));

    QVBoxLayout *thumbLayout = new QVBoxLayout(thumbFrame);
    thumbLayout->setContentsMargins(8, 8, 8, 8);
    thumbLayout->setSpacing(6);

    QLabel *thumbTitle = new QLabel(QStringLiteral("工件缩略图"), thumbFrame);
    thumbTitle->setProperty("styleClass", "header");
    thumbLayout->addWidget(thumbTitle);

    // Tab buttons: All / Favorites
    QHBoxLayout *tabRow = new QHBoxLayout();
    tabRow->setContentsMargins(0, 0, 0, 0);
    tabRow->setSpacing(6);

    m_tabAllBtn = new QPushButton(QStringLiteral("全部"), thumbFrame);
    m_tabAllBtn->setStyleSheet(kTabActiveStyle);
    m_tabAllBtn->setFixedHeight(60);
    m_tabAllBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    tabRow->addWidget(m_tabAllBtn, 1);

    m_tabFavBtn = new QPushButton(QStringLiteral("收藏夹"), thumbFrame);
    m_tabFavBtn->setStyleSheet(kTabInactiveStyle);
    m_tabFavBtn->setFixedHeight(60);
    m_tabFavBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    tabRow->addWidget(m_tabFavBtn, 1);

    thumbLayout->addLayout(tabRow);

    m_thumbnailList = new QListWidget(thumbFrame);
    m_thumbnailList->setViewMode(QListView::IconMode);
    m_thumbnailList->setMovement(QListView::Static);
    m_thumbnailList->setFlow(QListView::LeftToRight);
    m_thumbnailList->setResizeMode(QListView::Adjust);
    m_thumbnailList->setWrapping(true);
    m_thumbnailList->setSpacing(10);
    m_thumbnailList->setIconSize(QSize(150, 100));
    m_thumbnailList->setGridSize(QSize(kThumbnailGridWidth, kThumbnailGridHeight));
    m_thumbnailList->setWordWrap(true);
    m_thumbnailList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_thumbnailList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_thumbnailList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_thumbnailList->setMinimumWidth(kThumbnailGridWidth * kThumbnailColumnCount);
    m_thumbnailList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_thumbnailList->viewport()->installEventFilter(this);
    thumbLayout->addWidget(m_thumbnailList, 1);

    m_pageStack->addWidget(thumbFrame);
    m_pageStack->setCurrentIndex(0);

    // 鈹€鈹€ Connections 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€
    connect(m_thumbnailList, &QListWidget::currentRowChanged,
            this, &WorkpieceSelectionWidget::onThumbnailChanged);
    connect(m_pageSelectionBtn, &QPushButton::clicked,
            this, &WorkpieceSelectionWidget::onShowSelectionPage);
    connect(m_pageThumbnailBtn, &QPushButton::clicked,
            this, &WorkpieceSelectionWidget::onShowThumbnailPage);
    connect(m_searchBtn, &QPushButton::clicked,
            this, &WorkpieceSelectionWidget::onSearch);
    connect(m_searchEdit, &QLineEdit::returnPressed,
            this, &WorkpieceSelectionWidget::onSearch);
    connect(m_favoriteBtn, &QPushButton::clicked,
            this, &WorkpieceSelectionWidget::onToggleFavorite);
    connect(m_selectBtn, &QPushButton::clicked,
            this, &WorkpieceSelectionWidget::onSelectWorkpiece);
    connect(m_tabAllBtn, &QPushButton::clicked,
            this, &WorkpieceSelectionWidget::onShowAllTab);
    connect(m_tabFavBtn, &QPushButton::clicked,
            this, &WorkpieceSelectionWidget::onShowFavoritesTab);
    connect(m_categoryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &WorkpieceSelectionWidget::onCategoryChanged);

    schedulePreviewRefresh();
}

void WorkpieceSelectionWidget::loadModelMapping()
{
    m_idToProgramName.clear();
    m_idToDescription.clear();
    m_idToCategoryCode.clear();
    m_categoryNames.clear();
    m_categoryCodes.clear();

    const QString mappingPath = modelMappingFilePath();
    if (mappingPath.isEmpty()) {
        return;
    }

    QFile file(mappingPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    QTextStream in(&file);
    const QRegularExpression categoryLinePattern(QStringLiteral("^#\\s*(\\d{2})\\s*[:：]\\s*(.+)$"));
    const QRegularExpression idPattern(QStringLiteral("^\\d{2}\\d+$"));
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty()) {
            continue;
        }

        const QRegularExpressionMatch categoryMatch = categoryLinePattern.match(line);
        if (categoryMatch.hasMatch()) {
            const QString code = categoryMatch.captured(1);
            const QString name = categoryMatch.captured(2).trimmed();
            if (!m_categoryCodes.contains(code)) {
                m_categoryCodes.push_back(code);
            }
            m_categoryNames.insert(code, name.isEmpty() ? QStringLiteral("Category %1").arg(code) : name);
            continue;
        }

        const QStringList parts = line.split(QRegularExpression(QStringLiteral("\\s+")),
                                             Qt::SkipEmptyParts);
        if (parts.size() >= 2) {
            const QString id = parts.at(0);
            if (!idPattern.match(id).hasMatch()) {
                continue;
            }

            const QString programName = parts.at(1);
            const QString desc = parts.size() >= 3 ? parts.mid(2).join(QStringLiteral(" ")) : QString();
            const QString categoryCode = id.left(2);
            m_idToProgramName.insert(id, programName);
            m_idToCategoryCode.insert(id, categoryCode);
            if (!m_categoryCodes.contains(categoryCode)) {
                m_categoryCodes.push_back(categoryCode);
            }
            if (!m_categoryNames.contains(categoryCode)) {
                m_categoryNames.insert(categoryCode, QStringLiteral("Category %1").arg(categoryCode));
            }
            if (!desc.isEmpty()) {
                m_idToDescription.insert(id, desc);
            }
        }
    }

    refreshCategoryList();
}

void WorkpieceSelectionWidget::loadFavorites()
{
    m_favoriteIds.clear();

    QFile file(favoritesFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (!line.isEmpty()) {
            m_favoriteIds.insert(line);
        }
    }
}

void WorkpieceSelectionWidget::onSelectWorkpiece()
{
    if (m_currentId.isEmpty()) {
        QMessageBox::information(this,
                                 QStringLiteral("选择工件"),
                                 QStringLiteral("请先选择一个工件。"));
        return;
    }

    const QString programName = m_idToProgramName.value(m_currentId).trimmed();
    if (programName.isEmpty()) {
        QMessageBox::warning(this,
                             QStringLiteral("选择工件"),
                             QStringLiteral("model_mapping.txt 中未找到工件 ID %1 对应的程序名称。").arg(m_currentId));
        return;
    }

    emit workpieceProgramChosen(m_currentId, programName);
}

void WorkpieceSelectionWidget::saveFavorites()
{
    QFile file(favoritesFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return;
    }

    QTextStream out(&file);
    for (const QString &id : std::as_const(m_favoriteIds)) {
        out << id << "\n";
    }
}

void WorkpieceSelectionWidget::loadImages()
{
    m_allImagePaths.clear();

    QDir dir(imagesRootPath());
    const QFileInfoList files = dir.entryInfoList(imageFilters(), QDir::Files, QDir::Name);

    for (const QFileInfo &fileInfo : files) {
        QPixmap pixmap(fileInfo.absoluteFilePath());
        if (pixmap.isNull()) {
            continue;
        }
        m_allImagePaths.push_back(fileInfo.absoluteFilePath());
    }

    if (m_currentCategoryCode.isEmpty() && !m_categoryCodes.isEmpty()) {
        m_currentCategoryCode = m_categoryCodes.first();
        refreshCategoryList();
    }

    refreshThumbnailList();

    if (m_imagePaths.isEmpty()) {
        m_previewNameLabel->setText(QStringLiteral("未找到工件图片"));
        m_previewLabel->setPixmap(QPixmap());
        m_previewLabel->setText(QStringLiteral("请将工件图片放入 model 目录"));
        schedulePreviewRefresh();
        return;
    }

    m_thumbnailList->setCurrentRow(0);
    schedulePreviewRefresh();
}

void WorkpieceSelectionWidget::refreshCategoryList()
{
    if (!m_categoryCombo) {
        return;
    }

    const QString previousCode = m_currentCategoryCode;
    m_categoryCombo->blockSignals(true);
    m_categoryCombo->clear();
    for (const QString &code : std::as_const(m_categoryCodes)) {
        m_categoryCombo->addItem(QStringLiteral("%1 %2")
                                     .arg(code, m_categoryNames.value(code, QStringLiteral("Category %1").arg(code))),
                                 code);
    }
    m_categoryCombo->blockSignals(false);

    if (m_categoryCombo->count() <= 0) {
        m_currentCategoryCode.clear();
        return;
    }

    const int previousIndex = previousCode.isEmpty() ? -1 : m_categoryCombo->findData(previousCode);
    m_categoryCombo->setCurrentIndex(previousIndex >= 0 ? previousIndex : 0);
    m_currentCategoryCode = m_categoryCombo->currentData().toString();
    refreshThumbnailList();
    if (m_thumbnailList->count() > 0) {
        m_thumbnailList->setCurrentRow(0);
    }
}

void WorkpieceSelectionWidget::refreshThumbnailList()
{
    m_thumbnailList->blockSignals(true);
    m_thumbnailList->clear();
    m_imagePaths.clear();
    m_currentPixmap = QPixmap();

    for (const QString &path : std::as_const(m_allImagePaths)) {
        const QString baseName = QFileInfo(path).completeBaseName();
        const QString categoryCode = m_idToCategoryCode.value(baseName, baseName.left(2));

        if (!m_currentCategoryCode.isEmpty() && categoryCode != m_currentCategoryCode) {
            continue;
        }

        if (m_showingFavorites && !m_favoriteIds.contains(baseName)) {
            continue;
        }

        QPixmap pixmap(path);
        if (pixmap.isNull()) {
            continue;
        }

        QString displayText = baseName;
        const QString desc = m_idToDescription.value(baseName);

        if (!desc.isEmpty()) {
            displayText = baseName + QStringLiteral("\\n") + desc;
        }
        if (m_favoriteIds.contains(baseName)) {
            displayText = QStringLiteral("*") + displayText;
        }

        QListWidgetItem *item = new QListWidgetItem(
            QIcon(pixmap.scaled(150, 100, Qt::KeepAspectRatio, Qt::SmoothTransformation)),
            displayText);
        item->setSizeHint(QSize(180, 132));
        item->setToolTip(QFileInfo(path).fileName());
        item->setData(Qt::UserRole, baseName);
        m_thumbnailList->addItem(item);
        m_imagePaths.push_back(path);
    }

    m_thumbnailList->blockSignals(false);
}

void WorkpieceSelectionWidget::selectWorkpieceById(const QString &id)
{
    if (id.isEmpty()) {
        if (m_thumbnailList->count() > 0) {
            m_thumbnailList->setCurrentRow(0);
        }
        return;
    }

    for (int i = 0; i < m_thumbnailList->count(); ++i) {
        if (m_thumbnailList->item(i)->data(Qt::UserRole).toString() == id) {
            m_thumbnailList->setCurrentRow(i);
            return;
        }
    }

    // ID not found in current list 鈥?select first if available
    if (m_thumbnailList->count() > 0) {
        m_thumbnailList->setCurrentRow(0);
    } else {
        // Empty list
        m_currentPixmap = QPixmap();
        m_currentId.clear();
        m_previewNameLabel->setText(m_showingFavorites
            ? QStringLiteral("收藏夹为空")
            : QStringLiteral("未选择工件"));
        m_previewDescLabel->setText(QString());
        updateFavoriteButton();
        updatePreview();
    }
}

void WorkpieceSelectionWidget::selectAdjacentWorkpiece(int step)
{
    if (!m_thumbnailList || m_thumbnailList->count() <= 0 || step == 0) {
        return;
    }

    int nextRow = m_thumbnailList->currentRow();
    if (nextRow < 0) {
        nextRow = 0;
    } else {
        nextRow += step;
    }

    if (nextRow < 0) {
        nextRow = 0;
    }
    if (nextRow >= m_thumbnailList->count()) {
        nextRow = m_thumbnailList->count() - 1;
    }

    m_thumbnailList->setCurrentRow(nextRow);
}

void WorkpieceSelectionWidget::updateFavoriteButton()
{
    if (m_currentId.isEmpty()) {
        m_favoriteBtn->setText(QStringLiteral("收藏"));
        m_favoriteBtn->setStyleSheet(kFavOffStyle);
        m_favoriteBtn->setEnabled(false);
        if (m_selectBtn) {
            m_selectBtn->setEnabled(false);
        }
        return;
    }

    m_favoriteBtn->setEnabled(true);
    if (m_selectBtn) {
        m_selectBtn->setEnabled(true);
    }
    if (m_favoriteIds.contains(m_currentId)) {
        m_favoriteBtn->setText(QStringLiteral("已收藏"));
        m_favoriteBtn->setStyleSheet(kFavOnStyle);
    } else {
        m_favoriteBtn->setText(QStringLiteral("收藏"));
        m_favoriteBtn->setStyleSheet(kFavOffStyle);
    }
}

void WorkpieceSelectionWidget::updatePageSwitch()
{
    if (!m_pageStack || !m_pageSelectionBtn || !m_pageThumbnailBtn) {
        return;
    }

    const int currentIndex = m_pageStack->currentIndex();
    m_pageSelectionBtn->setStyleSheet(currentIndex == 0 ? kTabActiveStyle : kTabInactiveStyle);
    m_pageThumbnailBtn->setStyleSheet(currentIndex == 1 ? kTabActiveStyle : kTabInactiveStyle);
    for (int i = 0; i < m_extraPageButtons.size(); ++i) {
        m_extraPageButtons.at(i)->setStyleSheet(currentIndex == i + 2 ? kTabActiveStyle : kTabInactiveStyle);
    }
}

void WorkpieceSelectionWidget::layoutPreviewArea()
{
    if (!m_previewArea || !m_previewLabel) {
        return;
    }

    const QSize available = m_previewArea->size();
    if (available.width() <= 0 || available.height() <= 0) {
        return;
    }

    int targetWidth = available.width();
    int targetHeight = targetWidth * 3 / 4;
    if (targetHeight > available.height()) {
        targetHeight = available.height();
        targetWidth = targetHeight * 4 / 3;
    }

    m_previewLabel->setFixedSize(targetWidth, targetHeight);
}

void WorkpieceSelectionWidget::schedulePreviewRefresh()
{
    if (m_previewRefreshPending) {
        return;
    }

    m_previewRefreshPending = true;
    QTimer::singleShot(0, this, [this]() {
        m_previewRefreshPending = false;
        layoutPreviewArea();
        updatePreview();
    });
}

void WorkpieceSelectionWidget::updatePreview()
{
    if (!m_previewLabel) {
        return;
    }

    if (m_currentPixmap.isNull()) {
        if (m_allImagePaths.isEmpty()) {
            m_previewLabel->setPixmap(QPixmap());
            m_previewLabel->setText(QStringLiteral("请将工件图片放入 model 目录"));
        } else {
            m_previewLabel->setPixmap(QPixmap());
            m_previewLabel->setText(QStringLiteral("请选择一个工件"));
        }
        return;
    }

    const QSize targetSize = m_previewLabel->size();
    if (targetSize.width() <= 0 || targetSize.height() <= 0) {
        return;
    }

    m_previewLabel->setText(QString());
    m_previewLabel->setPixmap(buildCoverPixmap(m_currentPixmap, targetSize));
}

QPixmap WorkpieceSelectionWidget::buildCoverPixmap(const QPixmap& pixmap, const QSize& targetSize) const
{
    if (pixmap.isNull() || !targetSize.isValid()) {
        return QPixmap();
    }

    const QPixmap scaled = pixmap.scaled(targetSize,
                                         Qt::KeepAspectRatioByExpanding,
                                         Qt::SmoothTransformation);

    QPixmap result(targetSize);
    result.fill(Qt::transparent);

    QPainter painter(&result);
    const int offsetX = (scaled.width() - targetSize.width()) / 2;
    const int offsetY = (scaled.height() - targetSize.height()) / 2;
    painter.drawPixmap(0, 0, scaled, offsetX, offsetY, targetSize.width(), targetSize.height());
    return result;
}

QString WorkpieceSelectionWidget::imagesRootPath() const
{
    const QString appModelPath = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("model"));
    if (QDir(appModelPath).exists()) {
        return appModelPath;
    }

    const QString localModelPath = QDir(QDir::currentPath()).filePath(QStringLiteral("model"));
    if (QDir(localModelPath).exists()) {
        return localModelPath;
    }

    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../model"));
}

QString WorkpieceSelectionWidget::modelMappingFilePath() const
{
    const QStringList candidates = {
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("model/model_mapping.txt")),
        QDir(QDir::currentPath()).filePath(QStringLiteral("model/model_mapping.txt")),
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../model/model_mapping.txt"))
    };

    for (const QString &path : candidates) {
        if (QFileInfo::exists(path)) {
            return path;
        }
    }

    return QString();
}

QString WorkpieceSelectionWidget::favoritesFilePath() const
{
    // Save alongside the model directory
    const QStringList candidates = {
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("model")),
        QDir(QDir::currentPath()).filePath(QStringLiteral("model")),
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../model"))
    };

    for (const QString &dir : candidates) {
        if (QDir(dir).exists()) {
            return QDir(dir).filePath(QStringLiteral("favorites.txt"));
        }
    }

    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("model/favorites.txt"));
}
