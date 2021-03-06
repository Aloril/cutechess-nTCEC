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

#ifndef GAMEADJUDICATOR_H
#define GAMEADJUDICATOR_H

#include "board/result.h"
namespace Chess { class Board; }
class MoveEvaluation;

/*!
 * \brief A class for adjudicating chess games
 *
 * The GameAdjudicator class can be used to adjudicate chess games when
 * the probability of a specific result is high enough.
 */
class LIB_EXPORT GameAdjudicator
{
	public:
		/*!
		 * Creates a new game adjudicator.
		 *
		 * By default all adjudication is disabled.
		 */
		GameAdjudicator();

		/*!
		 * Sets the draw adjudication threshold for each game.
		 *
		 * A game will be adjudicated as a draw if both players report
		 * a score that's within \a score centipawns from zero for at
		 * least \a moveCount consecutive moves, and at least
		 * \a moveNumber full moves have been played.
		 */
		void setDrawThreshold(int moveNumber, int moveCount, int score);
		/*!
		 * Sets the resign adjudication threshold for each game.
		 *
		 * A game will be adjudicated as a loss for the player that
		 * made the last move if both players report a score that's at least
		 * \a score centipawns below zero for at least \a moveCount
		 * consecutive moves.
		 */
		void setResignThreshold(int moveCount, int score);
		/*!
		 * Sets tablebase adjudication to \a enable.
		 *
		 * If \a enable is true then games are adjudicated if the
		 * latest position is found in the tablebases.
		 */
		void setTablebaseAdjudication(bool enable);

		/*!
		 * Adds a new move evaluation to the adjudicator.
		 *
		 * \a board should be at the position that follows the move.
		 * \a eval should be the evaluation of the move.
		 * \a resetDraw should be set to true if board conditions mandate
		 *    that the draw count be reset (e.g. a pawn move was made)
		 *
		 * result() can be called after this function to find out if
		 * the game should be adjudicated.
		 */
		void addEval(const Chess::Board* board, const MoveEvaluation& eval, bool resetDraw = false);
		/*!
		 * Returns the adjudication result.
		 *
		 * This function returns the expected result of the game. If the
		 * game can't be adjudicated yet, a null result is returned.
		 */
		Chess::Result result() const;

		void resetDrawCount();

	private:
		int m_drawMoveNum;
		int m_drawMoveCount;
		int m_drawScore;
		int m_drawScoreCount;
		int m_resignMoveCount;
		int m_resignScore;
		int m_resignLoserScoreCount[2];
		int m_resignWinnerScoreCount[2];
		bool m_tbEnabled;
		Chess::Result m_result;
};

#endif // GAMEADJUDICATOR_H
