from flask import Flask
from config import Config
from models import db
from routes import api
import logging
import os

def create_app():
    """Create and configure Flask application"""
    app = Flask(__name__)
    app.config.from_object(Config)
    
    # Initialize database
    db.init_app(app)
    
    # Register blueprints
    app.register_blueprint(api)
    
    # Configure logging
    logging.basicConfig(
        level=logging.INFO,
        format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
    )
    
    # Create database tables
    with app.app_context():
        db.create_all()
        logging.info("Database tables created/verified")
    
    return app

if __name__ == '__main__':
    app = create_app()
    
    host = app.config.get('HOST', '0.0.0.0')
    port = app.config.get('PORT', 5000)
    
    logging.info(f"Starting Flask server on {host}:{port}")
    app.run(host=host, port=port, debug=True)
