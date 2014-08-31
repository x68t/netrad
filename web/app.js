'use strict';

(function() {
    var app = angular.module('app', ['ui.router', 'ngResource']);

    function strip(line) {
        return line.replace(/^\s+/, '').replace(/\s+$/, '');
    }

    function getHeaders(headers) {
        var k, v;
        var obj = {};

        angular.forEach(headers, function(line) {
            var i = line.indexOf(':');
            var o = {};
            if (i === -1)
                o[line] = '';
            else {
                k = strip(line.slice(0, i));
                v = strip(line.slice(i + 1));
                obj[k] = v;
            }
        });

        return obj;
    }

    app.config(function($stateProvider, $urlRouterProvider) {
        //$locationProvider.html5Mode(true);
        $urlRouterProvider.otherwise('/');
        $stateProvider
            .state('tune', {
                url: '/',
                templateUrl: 'templates/tune.html',
                controller: 'tuneController'
            })
            .state('register', {
                url: '/register',
                templateUrl: 'templates/register.html',
                controller: 'registerController'
            });
    });

    app.factory('volumeService', function($http) {
        var URL = 'api/volume';
        return {
            status: function(status) {
                if (arguments.length == 0)
                    return $http.get(URL);
                return $http.put(URL, status);
            }
        };
    });

    app.factory('tuneService', function($http) {
        return {
            set: function(station) {
                return $http.put('api/tune', station);
            },
            status: function() {
                return $http.get('api/status');
            },
            stop: function() {
                return $http.put('api/stop');
            }
        };
    });

    app.factory('stationService', function($resource) {
        return $resource('api/station/:id', { id: '@id' }, { update: { method: 'PUT' }});
    });

    app.controller('mainController', function($scope, volumeService, tuneService, stationService) {
        $scope.controllerName = 'mainController';
        $scope.stations = stationService.query();
        $scope.updateTuneStatus = function() {
            tuneService.status()
                .success(function(data) {
                    var status = {};
                    status.status = data.status;
                    status.headers = getHeaders(data.body);
                    $scope.tuneStatus = status;
                });
        };
        $scope.tuneStop = function() {
            tuneService.stop();
        };
        volumeService.status()
            .success(function(data) {
                $scope.volumeStatus = data;
            })
            .error(function(reson) {
                $scope.volumeStatus = reson;
            });
        $scope.$watch('volumeStatus', function(newValue, oldValue) {
            if (newValue !== undefined && oldValue !== undefined)
                volumeService.status(newValue);
        }, true);
        $scope.updateTuneStatus();
    });

    app.controller('tuneController', function($scope, tuneService) {
        $scope.controllerName = 'tuneController';
        $scope.tune = function(index) {
            tuneService.set($scope.stations[index]);
        };
    });

    app.controller('registerController', function($scope, $timeout, tuneService, stationService) {
        $scope.newStation = {};
        $scope.play = function() {
            tuneService.set($scope.newStation)
                .success(function(data) {
                    $timeout($scope.update, 5000);
                })
                .error(function() {
                    $scope.newStation = {};
                });
        };
        $scope.update = function() {
            tuneService.status()
                .success(function(data) {
                    var headers = getHeaders(data.body);
                    var icyName = headers['icy-name'] || '';
                    var i, newStation = { id: $scope.newStation.id, url: $scope.newStation.url };
                    if ((i = icyName.indexOf(' - ')) !== -1) {
                        newStation.site = icyName.slice(0, i);
                        newStation.program = icyName.slice(i + 3);
                    } else {
                        newStation.site = '';
                        newStation.program = icyName;
                    }
                    newStation.genre = headers['icy-genre'] || '';
                    $scope.newStation = newStation;
                });
        };
        $scope.select = function(index) {
            $scope.newStation = $scope.stations[index];
        };
        $scope.register = function() {
            if ($scope.newStation.id === undefined)
                stationService.save($scope.newStation);
            else
                stationService.update($scope.newStation);
            $scope.stations = stationService.query();
        };
        $scope.clear = function() {
            $scope.newStation = {};
        };
        $scope.remove = function() {
            stationService.remove($scope.newStation);
            $scope.stations = stationService.query();
        };
        $scope.isNew = function() {
            return $scope.newStation.id === undefined;
        };
        $scope.editIconClass = function() {
            return {
                "glyphicon-plus-sign": $scope.isNew(),
                "glyphicon-edit": !$scope.isNew()
            };
        };
    });

    app.filter('title', function() {
        return function(input) {
            if (typeof(input) !== 'string')
                return '';
            return input.replace(/^StreamTitle='/, '').replace(/';.*/, '');
        };
    });
})();
