#ifndef WORKPIECESELECTIONWIDGET_H
#define WORKPIECESELECTIONWIDGET_H

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMap>
#include <QPixmap>
#include <QPoint>
#include <QPushButton>
#include <QSet>
#include <QString>
#include <QWidget>

class QStackedWidget;

class WorkpieceSelectionWidget : public QWidget
{
    Q_OBJECT

public:
    explicit WorkpieceSelectionWidget(QWidget *parent = nullptr);
    QWidget* selectionActionBar() const;
    void addPage(const QString& title, QWidget *page);

signals:
    void workpieceProgramChosen(const QString& workpieceId, const QString& programName);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onThumbnailChanged(int row);
    void onSearch();
    void onToggleFavorite();
    void onSelectWorkpiece();
    void onShowAllTab();
    void onShowFavoritesTab();
    void onCategoryChanged(int index);
    void onShowSelectionPage();
    void onShowThumbnailPage();

private:
    void buildUi();
    void loadImages();
    void loadModelMapping();
    void loadFavorites();
    void saveFavorites();
    void refreshCategoryList();
    void refreshThumbnailList();
    void updateFavoriteButton();
    void updatePageSwitch();
    void layoutPreviewArea();
    void schedulePreviewRefresh();
    void updatePreview();
    void selectWorkpieceById(const QString &id);
    void selectAdjacentWorkpiece(int step);
    QPixmap buildCoverPixmap(const QPixmap& pixmap, const QSize& targetSize) const;
    QString imagesRootPath() const;
    QString favoritesFilePath() const;
    QString modelMappingFilePath() const;

    QWidget *m_previewArea = nullptr;
    QLabel *m_previewLabel = nullptr;
    QLabel *m_previewNameLabel = nullptr;
    QLabel *m_previewDescLabel = nullptr;
    QWidget *m_selectionActionBar = nullptr;
    QLabel *m_hintLabel = nullptr;

    QStackedWidget *m_pageStack = nullptr;
    QPushButton *m_pageSelectionBtn = nullptr;
    QPushButton *m_pageThumbnailBtn = nullptr;
    QList<QPushButton*> m_extraPageButtons;

    QLineEdit *m_searchEdit = nullptr;
    QPushButton *m_searchBtn = nullptr;
    QPushButton *m_favoriteBtn = nullptr;
    QPushButton *m_selectBtn = nullptr;
    QPushButton *m_tabAllBtn = nullptr;
    QPushButton *m_tabFavBtn = nullptr;
    QComboBox *m_categoryCombo = nullptr;

    QListWidget *m_thumbnailList = nullptr;
    QStringList m_allImagePaths;
    QStringList m_imagePaths;
    QPixmap m_currentPixmap;
    QPoint m_pressPos;
    bool m_draggingThumbList = false;
    bool m_draggingSwipe = false;
    bool m_previewRefreshPending = false;
    bool m_showingFavorites = false;
    QString m_currentId;
    QString m_currentCategoryCode;

    QMap<QString, QString> m_idToProgramName;
    QMap<QString, QString> m_idToDescription;
    QMap<QString, QString> m_idToCategoryCode;
    QMap<QString, QString> m_categoryNames;
    QStringList m_categoryCodes;
    QSet<QString> m_favoriteIds;
};

#endif // WORKPIECESELECTIONWIDGET_H
