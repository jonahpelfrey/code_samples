'use strict';

var Item = require('./models/item.js');
var User = require('./models/user.js');
var Game = require('./models/game.js');
var routes = require('./routes.js');

var request = require('request');
var path = require('path');
var morgan = require('morgan');
var crypto = require('crypto');
var bodyParser = require('body-parser');
var mongoose = require('mongoose');
var express = require('express');
var app = express();
var server = require('http').Server(app);

var io = require('socket.io')(server);

var tokens = [];

/* URLS */
const sports = 'https://opentdb.com/api.php?amount=25&category=21&difficulty=medium&type=multiple';
const music = 'https://opentdb.com/api.php?amount=25&category=12&difficulty=medium&type=multiple';
const geography = 'https://opentdb.com/api.php?amount=25&category=22&difficulty=medium&type=multiple';
const science = 'https://opentdb.com/api.php?amount=25&category=17&difficulty=medium&type=multiple';

/* INITIALIZE EXPRESS PARAMS */
var allowCrossDomain = function(req, res, next) {
    res.header('Access-Control-Allow-Origin', '*');
    res.header('Access-Control-Allow-Methods', 'GET,PUT,POST,DELETE');
    res.header('Access-Control-Allow-Headers', 'Origin, X-Requested-With, Content-Type, Accept');
    next();
}

app.set('port', process.env.PORT || 8000);
app.use(express.static(path.join(__dirname, './client')));
app.use(bodyParser.urlencoded({ extended: true }));
app.use(bodyParser.json());
app.use(morgan('dev'));
app.use(allowCrossDomain);

/*CONNECT TO DATABASE */
mongoose.connect('mongodb://localhost/trivia', function(error) {
	if(error) console.log(error);
	else console.log("| Connected to Trivia Database |");
});

/* SOCKET CONNECTIONS */
io.sockets.on('connection', function(socket) {

	console.log("User has connected");

	var user = new User();
	user.socket = socket.id;
	user.save(function(err) {
		if(err) console.log(err);
	});
	socket.send(socket.id);

	socket.on('disconnect', function() {
		console.log("User has disconnected");
		var token;
		User.findOne({socket: socket.id}, function(err, user) {
			if(err) console.log(err);
			else {
				token = user.token;
				Game.findOne({token: token}, function(err, game) {
					game.users-=1;
					game.save(function(err) {
						if(err) console.log(err);
					});
				});
			}
		});
		User.findOneAndRemove({socket: socket.id}, function(err, user) {
			if(err) console.log(err);
		});
		
	});

	socket.on('join.game', function(data) {
		console.log(socket.id);
		User.findOne({socket: socket.id}, function(err, user) {
			if(err) console.log(err);
			else {
				user.token = data.token;
				user.save(function(err) {
					if(err) console.log(err);
				});
			}
		});
		Game.findOne({token: data.token}, function(err, game) {
			if(err) console.log(err);
			else {
				game.users+=1;
				game.save(function(err){
					if(err) console.log(err);
				})
			}
		});
		socket.join(data.token);
		io.sockets.in(data.token).emit('player.joined');
	});

	socket.on('begin', function(data) {
		io.sockets.in(data.token).emit('startgame', "we are starting the game");
	});

	socket.on('cycle', function(data) {
		cycleCheck(data.token);
	});

	socket.on('new.game', function() {
		User.findOne({socket: socket.id}, function(err, user) {
			if(err) console.log(err);
			else {
				user.points = 0;
				user.name = '';
				user.save(function(err) {
					if(err) console.log(err);
				});
			}
		});
	});

	socket.on('submission', function(data) {
		User.findOne({socket: socket.id}, function(err, user) {
			if(err) console.log(err);
			else {
				user.points += data.points;
				user.save(function(err) {
					if(err) console.log(err);
				});
			}
		});
		Game.findOne({token: data.token}, function(err, game) {
			if(err) console.log(err);
			else {
				game.completed+=1;
				game.save(function(err) {
					if(err) console.log(err);
				});

				if(game.completed == game.users) {
					game.completed = 0;
					game.save(function(err) {
						if(err) console.log(err);
					});

					io.sockets.in(data.token).emit('end.round');
				}
			}
		});
	});
	
});

var cycleCheck = function(token) {
	Game.findOne({token: token}, function(err, game) {
		if(err) console.log(err);
		else {
			game.completion += 1;

			if(game.completion = game.users.length)
			{
				io.sockets.in(token).emit('nextround', "We are moving to the next round");
			}
		}
	});
}

/* ROUTER */
var router = express.Router();

router.route('/users/:token')
.get(function(req, res) {
	var token = req.params.token;
	User.find({ token: token }, function(err, users) {
		res.json(users);
	});
});

router.route('/user/name')
.post(function(req, res) {
	User.findOne({socket: req.body.socket}, function(err, user) {
		user.name = req.body.name;
		user.save(function(err, user) {
			if(err) res.send(err);
			else {
				res.send('successfully added user');
			}
		});
	});
});


/**
 * Retrieve a single question from the database, and remove it upon retrieval
 * @param NONE
 * @return 	{
				answer: 	(String) correct answer
				question: 	(String) the trivia question
				choices: 	[String] choices for answer, includes the correct answer and incorrect answers
 			}
 */
router.route('/next')
.get(function(req, res) {
	Item.find(function(err, items) {
		if(err) res.send(err);
		else {
			var active = items[0];
			Item.findByIdAndRemove(active._id, function(err, item) {
				if(err) res.send(err);
			});
			res.json(active); 
		}
	});
});

/**
 * Initializes the retrieval of questions, seeding of DB, and game creation / token creation
 * @param 	{ category: 	(String) Category of questions that you want to retrieve from the OpenTDB		}
 * @return 	{ token: (String) Token that identifies the game and can be used for a socket connection 	}
 */
router.route('/start')
.post(function(req, res) {

	var obj = [];
	var endp = '';

	switch(req.body.category) {
		case 'sports':
			endp = sports;
			break;
		case 'music':
			endp = music;
			break;
		case 'science':
			endp = science;
			break;
		case 'geography':
			endp = geography;
			break;
	}

	var options = {
		url: endp,
		method: 'GET',
	};

	request(options, function(err, res, body) {

		let questions = JSON.parse(body);
		
		for(var i =0; i < questions.results.length; i++) {

			var current = questions.results[i];
			obj.push(current.question);
			obj.push(current.correct_answer);
			obj.push.apply(obj, current.incorrect_answers);

			for(var j = 0; j < obj.length; j++) {

				if(obj[j].includes("&quot;")) {
					obj[j] = obj[j].replace(/&quot;/g, "'");			
				}

				if(obj[j].includes("&#039")) {
					obj[j] = obj[j].replace(/&#039;/g, "'");
				}
			}

			var item = new Item();
			item.question = obj.shift();
			item.choices.push.apply(item.choices, obj);
			item.choices.sort();
			item.answer = obj.shift();
			item.save(function(err) {
				if (err) res.send(err);
			});

			obj = [];
		}
		
	});

	// CREATE UNIQUE TOKEN, SAVE, RETURN RESPONSE
	var token = crypto.randomBytes(4).toString('hex');
	while(tokens.indexOf(token) >= 0)
	{
		token = crypto.randomBytes(4).toString('hex');
	}
	tokens.push(token);

	var game = new Game();
	game.token = token;
	game.save(function(err) {
		if(err) res.send(err);
	});

	res.json({ token: token });
});

app.use('/trivia', router);
app.use('/trivia', routes);

server.listen(app.get('port'), function() {
	console.log('Server running on port: 8000');
});