'use strict';

(function() {
    var app = angular.module('app', ['ui.router']);

    function strip(s) {
        return s.replace(/^\s+|\s+$/g, '');
    }

    function keyValue(s) {
        var i = s.indexOf(':');
        if (i === -1)
            return {
                key: s,
                value: ''
            };
        return {
            key: strip(s.slice(0, i)),
            value: strip(s.slice(i + 1))
        };
    }

    function netradService($http) {
        return {
            query: function() {
                return $http.get('api/stations');
            },
            status: function() {
                return $http.get('api/status');
            },
            tune: function(url) {
                return $http.put('api/tune/' + url);
            },
            stop: function() {
                return $http.put('api/stop');
            },
            volume: function(volume) {
                if (arguments.length === 0)
                    return $http.get('api/volume');
                return $http.put('api/volume', {volume: volume});
            }
        };
    }

    app.config(function($stateProvider, $urlRouterProvider) {
        $urlRouterProvider.otherwise('/main');
        $stateProvider
            .state('main', {
                url: '/main',
                templateUrl: 'templates/main.html',
                controller: 'mainController'
            });
    });

    app.controller('mainController', function($scope, $http, netradService) {
        $scope.h1 = 'mainController';
        $scope.headers = {};
        netradService.query()
            .success(function(data) {
                $scope.stations = data;
            })
            .error(function(data) {
                alert('error0');
            });
        netradService.volume()
            .success(function(data) {
                $scope.volume = data.volume;
            })
            .error(function(data) {
                alert('error1');
            });
        $scope.response = {};
        $scope.click = function(index) {
            var station = $scope.stations[index];
            netradService.tune(station.url)
                .error(function(data) {
                    alert('error2');
                });
        };
        $scope.stop = function() {
            netradService.stop()
                .error(function(data) {
                    alert('error3');
                });
        };
        $scope.reload = function() {
            netradService.status()
                .success(function(data) {
                    $scope.headers = {};
                    angular.forEach(data.body, function(line) {
                        var kv = keyValue(line);
                        $scope.headers[kv.key] = kv.value;
                    });
                })
                .error(function(data) {
                    alert('error4');
                });
        };

        $scope.reload();
        $scope.$watch('volume', function(newValue, oldValue) {
            if (isNaN(newValue = Number(newValue)))
                return;
            netradService.volume(newValue)
                .error(function(data) {
                    alert('error5: ' + newValue);
                });
        });
    });

    app.factory('netradService', netradService);
})();
