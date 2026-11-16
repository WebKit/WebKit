import * as CHESS from "./thirdparty/chess.min.js";

const rowMap = 'abcdefgh'.split('');
const columnMap = '12345678'.split('');
class Chess {
    #chess = new CHESS.Chess();
    #boardState = new Map();
    #history = [];
    #pieces;
    constructor (chessboardEl) {
        const whitePieces = Array.from(chessboardEl.shadowRoot.querySelectorAll("[part*=white]"));
        const blackPieces = Array.from(chessboardEl.shadowRoot.querySelectorAll("[part*=black]"));
        this.#pieces = [whitePieces, blackPieces].flat().map(el => {
            const summary = Array.from(el.part.values()).find(s => s.includes('chess')).split('_');
            const [, type, color] = summary;
            return {
                type, color,
                typeShort: type === 'knight' ? 'n' : type.slice(0,1),
                colorShort: color.slice(0,1),
                el,
                square: null
            };
        });

        this.#chess.reset();
        if (chessboardEl.object instanceof Promise) {
            chessboardEl.object.then(() => this.updateBoard(true));
        } else { 
            this.updateBoard(true);
        }
    }
    reset() {
        this.#history.length = 0;
        this.#chess.reset();
        this.updateBoard();
    }
    getSquareFromEl(needle) {
        const piece = this.#pieces.find(({el}) => needle === el);
        if (piece) {
            return piece.square;
        }
    }
    updateBoard(force) {
        const newBoardState = new Map();
        const piecesToPlace = this.#pieces.slice(0);

        for (const row of this.#chess.board()) {
            for (const squareData of row) {
                if (!squareData) continue;
                const {square, type, color} = squareData;
                let pieceIndex = -1;
                if (this.#boardState.has(square)) {
                    const elOnSquare = this.#boardState.get(square);
                    const piece = this.#pieces.find(({el}) => elOnSquare === el);
                    const {typeShort, colorShort} = piece;
                    if (typeShort === type && colorShort === color) {
                        pieceIndex = piecesToPlace.indexOf(piece);
                    }
                }
                if (pieceIndex === -1) pieceIndex = piecesToPlace.findIndex(({typeShort, colorShort}) => typeShort === type && colorShort === color);
                if (pieceIndex === -1) {
                    console.warn(`Could not find for ${square}: ${color}${type}`);
                    continue;
                }
                const [piece] = piecesToPlace.splice(pieceIndex,1);
                piece.square = square;
                const el = piece.el;
                newBoardState.set(square, el);
                el.style.setProperty('--visibility', 'visible');
                this.constructor.movePieceToSquare(el, square, force);
            }
        }

        for (const {el:takenPiece} of piecesToPlace) {
            takenPiece.style.setProperty('--visibility', 'hidden');
            takenPiece.constructor.markAsDirty(takenPiece);
        }
        this.#boardState = newBoardState;
    }

    fen() {
        return this.#chess.fen();
    }

    sync(chess) {
        this.reset();
        const fen = chess.fen();
        this.#history.splice(0,0,...this.#chess.history().map(move => move.san));
        this.#chess.load(fen);
        this.updateBoard();
    }

    async playToCompletion() {
        while(!this.#chess.isGameOver()) {
            await this.cpuNextMove(this.#chess.turn() === 'b' ? 12 : 10);
            await new Promise(r => setTimeout(r, 500));
        }
        console.log(this.checkEnding());
    }

    checkEnding() {
        const chess = this.#chess;
        if (chess.isGameOver()) {
            if (chess.isStalemate()) {
                return 'Stalemate';
            } else if (chess.isDraw()) {
                return 'Draw';
            } else {
                if (chess.turn() === 'b') {
                    return 'White wins';
                } else {
                    return 'Black wins';
                }
            }
        } else {
            return false;
        }
    }

    performMove(moveString) {
        const move = this.#chess.move(moveString);
        this.#history.push(moveString);

        const sq1 = move.from;
        const sq2 = move.to;
        const el = this.#boardState.get(sq1);
    
        if (!el) return console.log('Castling probably happened');
    
        this.#boardState.delete(sq1);
        if (this.#boardState.has(sq2)) {
            const takenPiece = this.#boardState.get(sq2);
            console.log(takenPiece.outerHTML, ' has been taken');
        }
    
        this.#boardState.set(sq2, el);
    }

    get chessState() {return this.#chess;}

    validateMove(moveString) {
        return !!this.#chess._moveFromSan(moveString);
    }
    
    async cpuNextMove(time=10) {
        try {
            // if the AI can't figure out a move that is legal then perform a random move.
            console.log('AI failure making random move');
            const chess = this.chessState;
            const moves = chess.moves({verbose:true});
            if (moves.length === 0) return console.log('No moves left to take.');
            const move = moves[Math.floor(Math.random() * moves.length)].lan;
            this.performMove(move);
            this.updateBoard();
        } catch(e) {
            console.log(e);
        }
    }

    /**
     * 
     * @param {HTMLElement} el 
     * @param {string} square 
     * @param {boolean} force 
     */
    static movePieceToSquare(el, square, force) {
        const [xPos, zPos] = el.style.getPropertyValue('--position').trim().split(' ');
        if (force || square !== this.squareFromPosition(xPos, zPos)) {
            let [row, column] = square;
            row = Number.parseInt(row, 36)-10; // 0-7
            column = Number.parseInt(column)-1; // 0-7
            const x = (column-4)*0.05715 + 0.028575;
            const z = (row-4)*0.05715 + 0.028575;
            el.part.add('animating');
            el.style.setProperty('--location', `${x} 0 ${z}`);
            setTimeout(() => el.part.remove('animating'), 550);
            el.constructor.markAsDirty(el);
        }
    }

    static squareFromPosition(x,y) {
        const column = Math.round((x - 0.028575)/0.05715 + 4);
        const row = Math.round((y - 0.028575)/0.05715 + 4);
        return rowMap[row] + columnMap[column];
    }
}

const chesses = new Map();
const scene = document.querySelector('sh-scene');
const chessgameGroup = document.getElementById('chess-group');

customElements.upgrade(window.chess3d);
await window.chess3d.object;
chesses.set(window.chess3d, new Chess(window.chess3d));

function endOfGame(state) {
    console.log(state);
    chessgameGroup.insertAdjacentHTML('beforeend', `<sh-group class="statemsg" rotate="0 -90 0" location="0 0.2 0" style="--text-box-width: 0.5177; --text-box-height: 0.16; --text-box-left: -0.25885; --text-box-right: 0.25885; --text-box-bottom: -0.24; --text-box-top: -0.08; --text-box-radius: 0.02;">
        <sh-use class="auto-text-shape" color="white" material="#screenmaterial">${state}</sh-use>
    </sh-group>`);
}
window.eog = endOfGame;

window.addEventListener('grabend', async function (e) {
    const board = e.target;
    const chess = chesses.get(board);
    const piece = e.detail.part;
    if (chess && e.detail.part) {
        const oldSquare = chess.getSquareFromEl(piece);
        const newSquare = Chess.squareFromPosition(piece.object.position.x, piece.object.position.z);
        const moveString = oldSquare + newSquare;
        if (chess.validateMove(moveString)) {
            chess.performMove(moveString);
            chess.updateBoard();
            if (chess.checkEnding()) {
                endOfGame(chess.checkEnding());
            } else {
                await chess.cpuNextMove(20);
                if (chess.checkEnding()) {
                    endOfGame(chess.checkEnding());
                }
            }
        } else {
            Chess.movePieceToSquare(piece, oldSquare, true);
            console.log('Invalid move');
        }
    }
});

window.resetgame.addEventListener('click', async function () {
    for (const el of document.getElementsByClassName('statemsg')) el.remove();
    for (const chess of chesses.values()) chess.reset();
});
for (const chess of chesses.values()) chess.reset();

/**
 * Sound effects
 * 
 * Firstly start background sound playing when the user starts interacting with the page
 */
window.addEventListener('click', function () {
    const clickSounds = document.getElementsByClassName('chessclick');
    const user = document.querySelector('sh-user');
    user.insertAdjacentHTML('afterbegin', '<sh-audio-listener slot="head"></sh-audio-listener>');
    const listener = user.querySelector('sh-audio-listener');

    // Don't spam sounds when it starts, just make sure they are set up and ready to go
    for (const sound of clickSounds) {
        sound.volume = 0.001;
        sound.play();
        sound.addEventListener("ended", () => {
            sound.volume = 1;
        }, {
            once: true
        });
    }

    window.magnetA.addEventListener('magnet-connect', function () {
        const index = Math.floor(Math.random() * clickSounds.length);
        listener.object.context.resume(); // hack for vision pro
        clickSounds[index].play();
    });
}, {
    once: true
});
