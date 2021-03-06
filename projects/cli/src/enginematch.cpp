/*
    This file is part of Cute Chess.

    Cute Chess is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Cute Chess is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Cute Chess.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "econode.h"
#include "board/board.h"

#include "enginematch.h"
#include <cmath>
#include <QMultiMap>
#include <QTextCodec>
#include <chessplayer.h>
#include <playerbuilder.h>
#include <chessgame.h>
#include <polyglotbook.h>
#include <tournament.h>
#include <gamemanager.h>
#include <sprt.h>

EngineMatch::EngineMatch(Tournament* tournament, QObject* parent)
	: QObject(parent),
	  m_tournament(tournament),
	  m_debug(false),
	  m_ratingInterval(0)
{
	Q_ASSERT(tournament != 0);

	m_startTime.start();
}

EngineMatch::~EngineMatch()
{
	qDeleteAll(m_books);
}

OpeningBook* EngineMatch::addOpeningBook(const QString& fileName)
{
	if (fileName.isEmpty())
		return 0;

	if (m_books.contains(fileName))
		return m_books[fileName];

	PolyglotBook* book = new PolyglotBook;
	if (!book->read(fileName))
	{
		delete book;
		qWarning("Can't read opening book file %s", qPrintable(fileName));
		return 0;
	}

	m_books[fileName] = book;
	return book;
}

void EngineMatch::start()
{
	connect(m_tournament, SIGNAL(finished()),
		this, SLOT(onTournamentFinished()));
	connect(m_tournament, SIGNAL(gameStarted(ChessGame*, int, int, int)),
		this, SLOT(onGameStarted(ChessGame*, int)));
	connect(m_tournament, SIGNAL(gameFinished(ChessGame*, int, int, int)),
		this, SLOT(onGameFinished(ChessGame*, int)));

	if (m_debug)
		connect(m_tournament->gameManager(), SIGNAL(debugMessage(QString)),
			this, SLOT(print(QString)));

	QMetaObject::invokeMethod(m_tournament, "start", Qt::QueuedConnection);
}

void EngineMatch::stop()
{
	QMetaObject::invokeMethod(m_tournament, "stop", Qt::QueuedConnection);
}

void EngineMatch::setDebugMode(bool debug)
{
	m_debug = debug;
}

void EngineMatch::setRatingInterval(int interval)
{
	Q_ASSERT(interval >= 0);
	m_ratingInterval = interval;
}

void EngineMatch::setTournamentFile(QString& tournamentFile)
{
	m_tournamentFile = tournamentFile;
}

void EngineMatch::generateSchedule(QVariantList& pList)
{
	QVariantMap pMap;

	QList< QPair<QString, QString> > pairings = m_tournament->getPairings();
	if (pairings.isEmpty()) return;

	int maxName = 5, maxTerm = 11, maxFen = 9;
	for (int i = 0; i < pList.size(); i++) {
		int len;
		pMap = pList.at(i).toMap();
		if (pMap.contains("terminationDetails")) {
			len = pMap["terminationDetails"].toString().length();
			if (len > maxTerm) maxTerm = len;
		}
		if (pMap.contains("finalFen")) {
			len = pMap["finalFen"].toString().length();
			if (len > maxFen) maxFen = len;
		}
	}

	// now check the player list for maxName
	int playerCount = m_tournament->playerCount();
	for (int i = 0; i < playerCount; i++) {
		int len = m_tournament->playerAt(i).builder->name().length();
		if (len > maxName) maxName = len;
	}

	QString scheduleFile(m_tournamentFile);
	QString scheduleText;

	scheduleFile = scheduleFile.remove(".json") + "_schedule.txt";

	scheduleText = QString("%1 %2 %3 %4 %5 %6 %7 %8 %9 %10 %11 %12 %13 %14\n")
		.arg("Nr", pairings.size() >= 100 ? 3 : 2)
		.arg("White", maxName)
		.arg("", 3)
		.arg("", -3)
		.arg("Black", -maxName)
		.arg("Termination", -maxTerm)
		.arg("Mov", 3)
		.arg("WhiteEv", 7)
		.arg("BlackEv", -7)
		.arg("Start", -22)
		.arg("Duration", 8)
		.arg("ECO", 3)
		.arg("FinalFen", -maxFen)
		.arg("Opening");

	if (!pairings.isEmpty()) {
		QList< QPair<QString, QString> >::iterator i;
		int count = 0;
		for (i = pairings.begin(); i != pairings.end(); ++i, ++count) {
			QString whiteName, blackName, whiteResult, blackResult, termination, startTime, duration, ECO, finalFen, opening;
			QString whiteEval, blackEval;
			QString plies = 0;

			whiteName = i->first;
			blackName = i->second;

			if (count < pList.size()) {
				pMap = pList.at(count).toMap();
				if (!pMap.isEmpty()) {
					if (pMap.contains("white")) // TODO error check against above
						whiteName = pMap["white"].toString();
					if (pMap.contains("black"))
						blackName = pMap["black"].toString();
					if (pMap.contains("startTime"))
						startTime = pMap["startTime"].toString();
					if (pMap.contains("result")) {
						QString result = pMap["result"].toString();
						if (result == "*") {
							whiteResult = blackResult = result;
						} else if (result == "1-0") {
							whiteResult = "1";
							blackResult = "0";
						} else if (result == "0-1") {
							blackResult = "1";
							whiteResult = "0";
						} else {
							whiteResult = blackResult = "1/2";
						}
					}
					if (pMap.contains("terminationDetails"))
						termination = pMap["terminationDetails"].toString();
					if (pMap.contains("gameDuration"))
						duration = pMap["gameDuration"].toString();
					if (pMap.contains("finalFen"))
						finalFen = pMap["finalFen"].toString();
					if (pMap.contains("ECO"))
						ECO = pMap["ECO"].toString();
					if (pMap.contains("opening"))
						opening = pMap["opening"].toString();
					if (pMap.contains("variation")) {
						QString variation = pMap["variation"].toString();
						if (!variation.isEmpty())
							opening += ", " + variation;
					}
					if (pMap.contains("plyCount"))
						plies = pMap["plyCount"].toString();
					if (pMap.contains("whiteEval"))
						whiteEval = pMap["whiteEval"].toString();
					if (pMap.contains("blackEval")) {
						blackEval = pMap["blackEval"].toString();
						if (blackEval.at(0) == '-') {
							blackEval.remove(0, 1);
						} else {
							if (blackEval != "0.00")
								blackEval = "-" + blackEval;
						}
					}
				}
			}
			scheduleText += QString("%1 %2 %3 %4 %5 %6 %7 %8 %9 %10 %11 %12 %13 %14\n")
				.arg(QString::number(count+1), pairings.size() >= 100 ? 3 : 2)
				.arg(whiteName, maxName)
				.arg(whiteResult, 3)
				.arg(blackResult, -3)
				.arg(blackName, -maxName)
				.arg(termination, -maxTerm)
				.arg(plies, 3)
				.arg(whiteEval, 7)
				.arg(blackEval, -7)
				.arg(startTime, -22)
				.arg(duration, 8)
				.arg(ECO, 3)
				.arg(finalFen, -maxFen)
				.arg(opening);
		}
		QFile output(scheduleFile);
		if (!output.open(QIODevice::WriteOnly | QIODevice::Text)) {
			qWarning("cannot open tournament configuration file: %s", qPrintable(scheduleFile));
		} else {
			QTextStream out(&output);
			out.setCodec(QTextCodec::codecForName("latin1")); // otherwise output is converted to ASCII
			out << scheduleText;
		}
	}
}

struct CrossTableData
{
public:

	CrossTableData(QString engineName, int elo = 0) :
		m_score(0),
		m_neustadtlScore(0),
		m_gamesPlayedAsWhite(0),
		m_gamesPlayedAsBlack(0),
		m_winsAsWhite(0),
		m_winsAsBlack(0)
	{
		m_engineName = engineName;
		m_elo = elo;
	};

	CrossTableData() :
	m_score(0),
	m_neustadtlScore(0),
	m_elo(0),
	m_gamesPlayedAsWhite(0),
	m_gamesPlayedAsBlack(0),
	m_winsAsWhite(0),
	m_winsAsBlack(0)
	{

	};

	bool isEmpty() { return m_engineName.isEmpty(); }

	QString m_engineName;
	QString m_engineAbbrev;
	double m_score;
	double m_neustadtlScore;
	int m_elo;
	int m_gamesPlayedAsWhite;
	int m_gamesPlayedAsBlack;
	int m_winsAsWhite;
	int m_winsAsBlack;
	QMap<QString, QString> m_tableData;
};

bool sortCrossTableDataByScore(const CrossTableData &s1, const CrossTableData &s2)
{
	if (s1.m_score == s2.m_score) {
		if (s1.m_neustadtlScore == s2.m_neustadtlScore) {
			if (s1.m_gamesPlayedAsBlack == s2.m_gamesPlayedAsBlack) {
				if ((s1.m_winsAsWhite + s1.m_winsAsBlack) == (s2.m_winsAsWhite + s2.m_winsAsBlack)) {
					return (s1.m_winsAsBlack > s2.m_winsAsBlack);
				} else {
					return (s1.m_winsAsWhite + s1.m_winsAsBlack) > (s2.m_winsAsWhite + s2.m_winsAsBlack);
				}
			} else {
				return s1.m_gamesPlayedAsBlack > s2.m_gamesPlayedAsBlack;
			}
		} else {
			return s1.m_neustadtlScore > s2.m_neustadtlScore;
		}
	}
	return s1.m_score > s2.m_score;
}

void EngineMatch::generateCrossTable(QVariantList& pList)
{
	int playerCount = m_tournament->playerCount();
	QMap<QString, CrossTableData> ctMap;
	QStringList abbrevList;
	int roundLength = 2;
	int maxName = 6;

	// ensure names and abbreviations
	for (int i = 0; i < playerCount; i++) {
		CrossTableData ctd(m_tournament->playerAt(i).builder->name(), m_tournament->playerAt(i).builder->rating());
		if (ctd.m_engineName.length() > maxName) maxName = ctd.m_engineName.length();

		int n = 1;
		QString abbrev;
		abbrev.append(ctd.m_engineName.at(0).toUpper()).append(ctd.m_engineName.length() > n ? ctd.m_engineName.at(n++).toLower() : ' ');
		while (abbrevList.contains(abbrev)) {
			abbrev[1] = ctd.m_engineName.length() > n ? ctd.m_engineName.at(n++).toLower() : ' ';
		}
		ctd.m_engineAbbrev = abbrev;
		abbrevList.append(abbrev);
		ctMap.insert(ctd.m_engineName, ctd);
	}

	// calculate scores and crosstable strings
	for (int i = 0; i < pList.size(); i++) {
		QVariantMap pMap = pList.at(i).toMap();
		if (pMap.contains("white") && pMap.contains("black") && pMap.contains("result")) {
			QString whiteName = pMap["white"].toString();
			QString blackName = pMap["black"].toString();
			QString result = pMap["result"].toString();
			CrossTableData& whiteData = ctMap[whiteName];
			CrossTableData& blackData = ctMap[blackName];
			QString& whiteDataString = whiteData.m_tableData[blackName];
			QString& blackDataString = blackData.m_tableData[whiteName];

			if (result == "*") {
				continue; // game in progress or invalid or something
			}
			if (result == "1-0") {
				whiteData.m_score += 1;
				whiteData.m_winsAsWhite++;
				whiteDataString += "1";
				blackDataString += "0";
			} else if (result == "0-1") {
				blackData.m_score += 1;
				blackData.m_winsAsBlack++;
				whiteDataString += "0";
				blackDataString += "1";
			} else if (result == "1/2-1/2") {
				whiteData.m_score += 0.5;
				blackData.m_score += 0.5;
				whiteDataString += "=";
				blackDataString += "=";
			}
			if (whiteDataString.length() > roundLength) roundLength = whiteDataString.length();
			if (blackDataString.length() > roundLength) roundLength = blackDataString.length();
			whiteData.m_gamesPlayedAsWhite++;
			blackData.m_gamesPlayedAsBlack++;
		}
	}
	// calculate SB
	QMapIterator<QString, CrossTableData> ct(ctMap);
	double largestSB = 0;
	double largestScore = 0;
	while (ct.hasNext()) {
		ct.next();
		CrossTableData& ctd = ctMap[ct.key()];
		QMapIterator<QString, QString> td(ctd.m_tableData);
		double sb = 0;
		while (td.hasNext()) {
			td.next();
			QString::ConstIterator c = td.value().begin();
			while (c != td.value().end()) {
				if (*c == QChar('1')) {
					sb += ctMap[td.key()].m_score;
				} else if (*c == QChar('=')) {
					sb += ctMap[td.key()].m_score / 2.;
				}
				c++;
			}
		}
		ctd.m_neustadtlScore = sb;
		if (ctd.m_neustadtlScore > largestSB) largestSB = ctd.m_neustadtlScore;
		if (ctd.m_score > largestScore) largestScore = ctd.m_score;
	}

	if (playerCount == 2) {
		roundLength = 2;
		QVariantMap pMap = pList.at(0).toMap();
		if (pMap.contains("white") && pMap.contains("black")) {
			QString whiteName = pMap["white"].toString();
			QString blackName = pMap["black"].toString();
			CrossTableData& whiteData = ctMap[whiteName];
			CrossTableData& blackData = ctMap[blackName];
			QString& whiteDataString = whiteData.m_tableData[blackName];
			QString& blackDataString = blackData.m_tableData[whiteName];
			int whiteWin = 0;
			int whiteLose = 0;
			int whiteDraw = 0;

			for (int i = 0; i < whiteDataString.length(); i++) {
				if (whiteDataString[i] == '1')
					whiteWin++;
				else if (whiteDataString[i] == '0')
					whiteLose++;
				else
					whiteDraw++;
			}
			whiteDataString = QString("+ %1 = %2 - %3")
				.arg(whiteWin)
				.arg(whiteDraw)
				.arg(whiteLose);
			blackDataString = QString("+ %1 = %2 - %3")
				.arg(whiteLose)
				.arg(whiteDraw)
				.arg(whiteWin);

			if (whiteDataString.length() > roundLength) roundLength = whiteDataString.length();
			if (blackDataString.length() > roundLength) roundLength = blackDataString.length();
		}
	}

	int maxScore = largestScore >= 100 ? 5 : largestScore >= 10 ? 4 : 3;
	int maxSB = largestSB >= 100 ? 6 : largestSB >= 10 ? 5 : 4;
	int maxGames = m_tournament->currentRound() >= 100 ? 4 : m_tournament->currentRound() >= 10 ? 3 : 2;
	QString crossTableHeaderText = QString("%1 %2 %3 %4 %5 %6")
		.arg("N", 2)
		.arg("Engine", -maxName)
		.arg("Rtng", -4)
		.arg("Pts", maxScore)
		.arg("Gm", maxGames)
		.arg("SB", maxSB);

	QString crossTableBodyText;

	QList<CrossTableData> list = ctMap.values();
	qSort(list.begin(), list.end(), sortCrossTableDataByScore);
	QList<CrossTableData>::iterator i;
	int count = 1;
	for (i = list.begin(); i != list.end(); ++i, ++count) {
		crossTableHeaderText += QString(" %1").arg(i->m_engineAbbrev, -roundLength);

		crossTableBodyText += QString("%1 %2 %3 %4 %5 %6")
			.arg(count, 2)
			.arg(i->m_engineName, -maxName)
			.arg(i->m_elo, 4)
			.arg(i->m_score, maxScore, 'f', 1)
			.arg(i->m_gamesPlayedAsWhite + i->m_gamesPlayedAsBlack, maxGames)
			.arg(i->m_neustadtlScore, maxSB, 'f', 2);

		QList<CrossTableData>::iterator j;
		for (j = list.begin(); j != list.end(); ++j) {
			if (j->m_engineName == i->m_engineName) {
				crossTableBodyText += " ";
				int rl = roundLength;
				while(rl--) crossTableBodyText += "\u00B7";
			} else crossTableBodyText += QString(" %1").arg(i->m_tableData[j->m_engineName], -roundLength);
		}
		crossTableBodyText += "\n";
	}

	QString crossTableText = crossTableHeaderText + "\n\n" + crossTableBodyText;

	QString crossTableFile(m_tournamentFile);
	crossTableFile = crossTableFile.remove(".json") + "_crosstable.txt";

	QFile output(crossTableFile);
	if (!output.open(QIODevice::WriteOnly | QIODevice::Text)) {
		qWarning("cannot open tournament configuration file: %s", qPrintable(crossTableFile));
	} else {
		QTextStream out(&output);
		out.setCodec(QTextCodec::codecForName("latin1")); // otherwise output is converted to ASCII
		out << crossTableText;
	}
}

void EngineMatch::onGameStarted(ChessGame* game, int number)
{
	Q_ASSERT(game != 0);

	qDebug("Started game %d of %d (%s vs %s)",
		   number,
		   m_tournament->finalGameCount(),
		   qPrintable(game->player(Chess::Side::White)->name()),
		   qPrintable(game->player(Chess::Side::Black)->name()));

	if (!m_tournamentFile.isEmpty()) {
		QVariantMap tfMap;
		if (QFile::exists(m_tournamentFile)) {
			QFile input(m_tournamentFile);
			if (!input.open(QIODevice::ReadOnly | QIODevice::Text)) {
				qWarning("cannot open tournament configuration file: %s", qPrintable(m_tournamentFile));
				return;
			}

			QTextStream stream(&input);
			JsonParser jsonParser(stream);
			tfMap = jsonParser.parse().toMap();
		}

		QVariantList pList;
		if (!tfMap.isEmpty()) {
			if (tfMap.contains("matchProgress")) {
				pList = tfMap["matchProgress"].toList();
				int length = pList.length();
				if (length >= number) {
					qWarning("game %d already exists, deleting", number);
					while(length-- >= number) {
						pList.removeLast();
					}
				}
			}
		}

		QVariantMap pMap;
		pMap.insert("index", number);
		pMap.insert("white", game->player(Chess::Side::White)->name());
		pMap.insert("black", game->player(Chess::Side::Black)->name());
		QDateTime qdt = QDateTime::currentDateTime();
		pMap.insert("startTime", qdt.toString("HH:mm:ss' on 'yyyy.MM.dd"));
		pMap.insert("result", "*");
		pMap.insert("terminationDetails", "in progress");
		pList.append(pMap);
		tfMap.insert("matchProgress", pList);
		{
			QFile output(m_tournamentFile);
			if (!output.open(QIODevice::WriteOnly | QIODevice::Text)) {
				qWarning("cannot open tournament configuration file: %s", qPrintable(m_tournamentFile));
			} else {
				QTextStream out(&output);
				JsonSerializer serializer(tfMap);
				serializer.serialize(out);
			}
		}
		generateSchedule(pList);
		generateCrossTable(pList);
	}
}

void EngineMatch::onGameFinished(ChessGame* game, int number)
{
	Q_ASSERT(game != 0);

	Chess::Result result(game->result());
	qDebug("Finished game %d (%s vs %s): %s",
		   number,
		   qPrintable(game->player(Chess::Side::White)->name()),
		   qPrintable(game->player(Chess::Side::Black)->name()),
		   qPrintable(result.toVerboseString()));

		if (!m_tournamentFile.isEmpty()) {
			QVariantMap tfMap;

			if (QFile::exists(m_tournamentFile)) {
			QFile input(m_tournamentFile);
			if (!input.open(QIODevice::ReadOnly | QIODevice::Text)) {
				qWarning("cannot open tournament configuration file: %s", qPrintable(m_tournamentFile));
			} else {
				QTextStream stream(&input);
				JsonParser jsonParser(stream);
				tfMap = jsonParser.parse().toMap();
			}

			QVariantMap pMap;
			QVariantList pList;

			if (!tfMap.isEmpty()) {
				if (tfMap.contains("matchProgress")) {
					pList = tfMap["matchProgress"].toList();
					int length = pList.length();
					if (length < number) {
						qWarning("game %d doesn't exist", number);
					} else
						pMap = pList.at(number-1).toMap();
				}
			}

			if (!pMap.isEmpty()) {
				pMap.insert("result", result.toShortString());
				pMap.insert("terminationDetails", result.shortDescription());
				PgnGame *pgn = game->pgn();
				if (pgn) {
					const EcoInfo eco = pgn->eco();
					QString val;
					val = eco.ecoCode();
					if (!val.isEmpty()) pMap.insert("ECO", val);
					val = eco.opening();
					if (!val.isEmpty()) pMap.insert("opening", val);
					val = eco.variation();
					if (!val.isEmpty()) pMap.insert("variation", val);
					// TODO: after TCEC is over, change this to moveCount, since that's what it is
					pMap.insert("plyCount", qRound(game->moves().size() / 2.));
				}
				pMap.insert("finalFen", game->board()->fenString());

				MoveEvaluation eval;
				QString sScore;
				Chess::Side sides[] = { Chess::Side::White, Chess::Side::Black, Chess::Side::NoSide };

				for (int i = 0; sides[i] != Chess::Side::NoSide; i++) {
					Chess::Side side = sides[i];
					MoveEvaluation eval = game->player(side)->evaluation();
					int score = eval.score();
					int absScore = qAbs(score);
					QString sScore;

					// Detect mate-in-n scores
					if (absScore > 9900
					&&	(absScore = 1000 - (absScore % 1000)) < 100)
					{
						if (score < 0)
							sScore = "-";
						sScore += "M" + QString::number(absScore);
					}
					else
						sScore = QString::number(double(score) / 100.0, 'f', 2);

					if (side == Chess::Side::White)
						pMap.insert("whiteEval", sScore);
					else
						pMap.insert("blackEval", sScore);
				}

				pMap.insert("gameDuration", game->gameDuration());
				pList.replace(number-1, pMap);
				tfMap.insert("matchProgress", pList);

				QFile output(m_tournamentFile);
				if (!output.open(QIODevice::WriteOnly | QIODevice::Text)) {
					qWarning("cannot open tournament configuration file: %s", qPrintable(m_tournamentFile));
				} else {
					QTextStream out(&output);
					JsonSerializer serializer(tfMap);
					serializer.serialize(out);
				}
				generateSchedule(pList);
				generateCrossTable(pList);
			}
		}
	}

	if (m_tournament->playerCount() == 2)
	{
		Tournament::PlayerData fcp = m_tournament->playerAt(0);
		Tournament::PlayerData scp = m_tournament->playerAt(1);
		int totalResults = fcp.wins + fcp.losses + fcp.draws;
		qDebug("Score of %s vs %s: %d - %d - %d	 [%.3f] %d",
			   qPrintable(fcp.builder->name()),
			   qPrintable(scp.builder->name()),
			   fcp.wins, scp.wins, fcp.draws,
			   double(fcp.wins * 2 + fcp.draws) / (totalResults * 2),
			   totalResults);
	}

	if (m_ratingInterval != 0
	&&  (m_tournament->finishedGameCount() % m_ratingInterval) == 0)
		printRanking();
}

void EngineMatch::onTournamentFinished()
{
	if (m_ratingInterval == 0
	||  m_tournament->finishedGameCount() % m_ratingInterval != 0)
		printRanking();

	QString error = m_tournament->errorString();
	if (!error.isEmpty())
		qWarning("%s", qPrintable(error));
	else
	{
		switch (m_tournament->sprt()->status())
		{
		case Sprt::AcceptH0:
			qDebug("SPRT: H0 was accepted");
			break;
		case Sprt::AcceptH1:
			qDebug("SPRT: H1 was accepted");
			break;
		default:
			break;
		}
	}

	qDebug("Finished match");
	connect(m_tournament->gameManager(), SIGNAL(finished()),
		this, SIGNAL(finished()));
	m_tournament->gameManager()->finish();
}

void EngineMatch::print(const QString& msg)
{
	qDebug() << m_startTime.elapsed() << " " << qPrintable(msg);
}

struct RankingData
{
	QString name;
	int games;
	qreal score;
	qreal draws;
};

void EngineMatch::printRanking()
{
	QMultiMap<qreal, RankingData> ranking;

	for (int i = 0; i < m_tournament->playerCount(); i++)
	{
		Tournament::PlayerData player(m_tournament->playerAt(i));

		int score = player.wins * 2 + player.draws;
		int total = (player.wins + player.losses + player.draws) * 2;
		if (total <= 0)
			continue;

		qreal ratio = qreal(score) / qreal(total);
		qreal eloDiff = -400.0 * std::log(1.0 / ratio - 1.0) / std::log(10.0);

		if (m_tournament->playerCount() == 2)
		{
			qDebug("ELO difference: %.0f", eloDiff);
			break;
		}

		RankingData data = { player.builder->name(),
							 total / 2,
							 ratio,
							 qreal(player.draws * 2) / qreal(total) };
		ranking.insert(-eloDiff, data);
	}

	if (!ranking.isEmpty())
		qDebug("%4s %-23s %7s %7s %7s %7s",
			   "Rank", "Name", "ELO", "Games", "Score", "Draws");

	int rank = 0;
	QMultiMap<qreal, RankingData>::const_iterator it;
	for (it = ranking.constBegin(); it != ranking.constEnd(); ++it)
	{
		const RankingData& data = it.value();
		qDebug("%4d %-23s %7.0f %7d %6.0f%% %6.0f%%",
			   ++rank,
			   qPrintable(data.name),
			   -it.key(),
			   data.games,
			   data.score * 100.0,
			   data.draws * 100.0);
	}
}
