#include "attributeTabItem.h"

attributeTabItem::attributeTabItem(TabType tabType, int tabNum, QWidget *parent) : QWidget(parent)
{
    setContentsMargins(0,0,0,0);

    theGrid = new QGridLayout(this);
    theGrid->setHorizontalSpacing(4);
    theGrid->setVerticalSpacing(2);
    setLayout(theGrid);
    int row = 0, column = 0;

    if(tabType == gruepr)
    {
        attributeText = new QTextEdit(this);
        attributeText->setFrameStyle(QFrame::StyledPanel | QFrame::Plain);
        attributeText->setContextMenuPolicy(Qt::NoContextMenu);
        attributeText->setAcceptDrops(false);
        attributeText->setReadOnly(true);
        attributeText->setUndoRedoEnabled(false);
        attributeText->setEnabled(true);
        attributeText->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        theGrid->addWidget(attributeText, row++, column, 1, -1);

        weightPreLabel = new QLabel(tr("Weight:"), this);
        theGrid->addWidget(weightPreLabel, row, column++);

        weight = new QDoubleSpinBox(this);
        weight->setToolTip(tr("<html>The relative importance of this attribute in forming the teams.</html>"));
        weight->setButtonSymbols(QAbstractSpinBox::NoButtons);
        weight->setDecimals(1);
        weight->setMinimum(0);
        weight->setMaximum(TeamingOptions::MAXWEIGHT);
        weight->setValue(1);
        theGrid->addWidget(weight, row, column++);
        weight->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        weight->setFixedSize(weight->sizeHint());

        weightPostLabel = new QLabel("/" + QString::number(TeamingOptions::MAXWEIGHT) + "   ", this);
        theGrid->addWidget(weightPostLabel, row, column++);

        homogeneous = new QCheckBox(tr("Prefer\nHomogeneous"), this);
        homogeneous->setToolTip(tr("If selected, all of the students on a team will have a similar response to this question.\n"
                                   "If unselected, the students on a team will have a wide range of responses to this question."));
        theGrid->addWidget(homogeneous, row, column++);
        homogeneous->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        homogeneous->setFixedSize(homogeneous->sizeHint());

        requiredButton = new QPushButton(tr("Required\nAttributes"), this);
        requiredButton->setToolTip(tr("<html>Indicate attribute value(s) where each team should have at least one student with that value.</html>"));
        theGrid->addWidget(requiredButton, row, column++);
        requiredButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        requiredButton->setFixedSize(requiredButton->sizeHint());

        incompatsButton = new QPushButton(tr("Incompatible\nAttributes"), this);
        incompatsButton->setToolTip(tr("<html>Indicate attribute value(s) that should prevent students from being on the same team.</html>"));
        theGrid->addWidget(incompatsButton, row, column);
        incompatsButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        incompatsButton->setFixedSize(incompatsButton->sizeHint());
    }
    else    // surveymaker
    {
        closeButton = new QPushButton(QIcon(":/icons/delete.png"), tr("Delete"), this);
        closeButton->setToolTip(tr("<html>Delete this question from the survey. (This cannot be undone.)</html>"));
        connect(closeButton, &QPushButton::clicked, this, &attributeTabItem::closeButtonPushed);
        theGrid->addWidget(closeButton, row++, column, 1, -1, Qt::AlignRight);

        attributeText = new QTextEdit(this);
        attributeText->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
        attributeText->setAcceptDrops(true);
        attributeText->setReadOnly(false);
        attributeText->setUndoRedoEnabled(true);
        attributeText->setEnabled(true);
        attributeText->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        theGrid->addWidget(attributeText, row++, column, 1, -1);

        attributeResponses = new ComboBoxWithElidedContents(tr("Very high / Above average / Average / Below average / Very low"), this);
        theGrid->addWidget(attributeResponses, row++, column, 1, -1);
        allowMultipleResponses = new QCheckBox(tr("Allow student to select multiple options"), this);
        theGrid->addWidget(allowMultipleResponses, row++, column, 1, -1);

        attributeResponses->addItem("Choose the response options...");
        attributeResponses->insertSeparator(1);
        QStringList responseOptions = QString(RESPONSE_OPTIONS).split(';');
        for(int response = 0; response < responseOptions.size(); response++)
        {
            attributeResponses->addItem(responseOptions.at(response));
            attributeResponses->setItemData(response + 2, responseOptions.at(response), Qt::ToolTipRole);
        }

        setTabNum(tabNum);
    }
}


void attributeTabItem::setValues(int attribute, const DataOptions *const dataOptions, TeamingOptions *teamingOptions)
{
    if(attribute >= dataOptions->numAttributes)
    {
        attributeText->setHtml(tr("<html>N/A</html>"));
        weight->setEnabled(false);
        homogeneous->setEnabled(false);
        requiredButton->setEnabled(false);
        incompatsButton->setEnabled(false);
        return;
    }

    updateQuestionAndResponses(attribute, dataOptions);

    if(dataOptions->attributeVals[attribute].size() == 1)
    {
        teamingOptions->attributeWeights[attribute] = 0;
        weight->setEnabled(false);
        weight->setToolTip(tr("With only one response value, this attribute cannot be used for teaming"));
        homogeneous->setEnabled(false);
        homogeneous->setToolTip(tr("With only one response value, this attribute cannot be used for teaming"));
        requiredButton->setEnabled(false);
        requiredButton->setToolTip(tr("With only one response value, this attribute cannot be used for teaming"));
        incompatsButton->setEnabled(false);
        incompatsButton->setToolTip(tr("With only one response value, this attribute cannot be used for teaming"));
    }
    else
    {
        weight->setEnabled(true);
        weight->setToolTip(tr("The relative importance of this attribute in forming the teams"));
        homogeneous->setEnabled(true);
        homogeneous->setToolTip(tr("If selected, all of the students on a team will have a similar response to this question.\n"
                                   "If unselected, the students on a team will have a wide range of responses to this question."));
        requiredButton->setEnabled(true);
        requiredButton->setToolTip(tr("<html>Indicate attribute value(s) where each team should have at least one student with that value.</html>"));
        incompatsButton->setEnabled(true);
        incompatsButton->setToolTip(tr("<html>Indicate attribute value(s) that should prevent students from being on the same team.</html>"));
    }
    weight->setValue(double(teamingOptions->attributeWeights[attribute]));
    homogeneous->setChecked(teamingOptions->desireHomogeneous[attribute]);
}

void attributeTabItem::updateQuestionAndResponses(int attribute, const DataOptions *const dataOptions, const std::map<QString, int> &responseCounts)
{
    const auto type = dataOptions->attributeType[attribute];

    QString questionWithResponses = "<html>" + dataOptions->attributeQuestionText.at(attribute);
    if(type == DataOptions::multicategorical)
    {
        questionWithResponses += "<br><i>Multiple responses allowed.</i>";
    }
    questionWithResponses += "<hr><div style=\"margin-left:5%;\">";

    QRegularExpression startsWithInteger(R"(^(\d++)([\.\,]?$|[\.\,]\D|[^\.\,]))");
    QRegularExpression GMToffset("(.*)\\[GMT(.*):(.*)\\].*");  // characters after "[GMT" are +hh:mm "]"
    int responseNum = 0;
    for(const auto &response : qAsConst(dataOptions->attributeQuestionResponses[attribute]))
    {
        if(type == DataOptions::ordered)
        {
            // show response with starting number in bold
            QRegularExpressionMatch match = startsWithInteger.match(response);
            questionWithResponses += "<br><b>" + match.captured(1) + "</b>" + response.mid(match.capturedLength(1));
        }
        else if((type == DataOptions::categorical) || (type == DataOptions::multicategorical))
        {
            // show response with a preceding letter in bold (letter repeated for responses after 26)
            questionWithResponses += "<br><b>";
            questionWithResponses += (responseNum < 26 ? QString(char(responseNum + 'A')) : QString(char(responseNum%26 + 'A')).repeated(1 + (responseNum/26)));
            questionWithResponses += "</b>. " + response;
        }
        else
        {
            // timezone, show response with GMT in bold
            QRegularExpressionMatch offset = GMToffset.match(response);
            if(offset.hasMatch())
            {
                QString timezoneText = offset.captured(1);
                int hours = offset.captured(2).toInt();
                int minutes = offset.captured(3).toInt();
                QString GMTtext = QString("%1%2:%3").arg(hours >= 0 ? "+" : "").arg(hours).arg(minutes, 2, 10, QChar('0'));
                questionWithResponses += "<br><b>" + GMTtext + "</b> " + timezoneText;
            }
            else
            {
                questionWithResponses += "<br><b>";
                questionWithResponses += (responseNum < 26 ?
                                           QString(char(responseNum + 'A')) :
                                           QString(char(responseNum%26 + 'A')).repeated(1 + (responseNum/26)));
                questionWithResponses += "</b>. " + response;
            }
        }
        questionWithResponses += "  (" +
                                 QString::number(responseCounts.empty() ?
                                                 (dataOptions->attributeQuestionResponseCounts[attribute].at(response)) :
                                                 (responseCounts.at(response)))
                                 + " " + tr("students") + ")";
        responseNum++;
    }
    questionWithResponses += "</div>";
    questionWithResponses += "</html>";
    attributeText->setHtml(questionWithResponses);
}

void attributeTabItem::setTabNum(int tabNum)
{
    index = tabNum;
    attributeText->setPlaceholderText(tr("Enter attribute question ") + QString::number(tabNum + 1));
}

void attributeTabItem::closeButtonPushed()
{
    emit closeRequested(index);
}
